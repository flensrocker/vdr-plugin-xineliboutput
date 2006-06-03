/*
 * menu.c: Main Menu
 *
 * See the main source file 'xineliboutput.c' for copyright information and
 * how to reach the author.
 *
 * $Id: menu.c,v 1.1 2006-06-03 10:01:17 phintuka Exp $
 *
 */

#include <dirent.h>

#include <vdr/config.h>
#include <vdr/i18n.h>
#include <vdr/interface.h>
#include <vdr/menu.h>
#include <vdr/plugin.h>

#include "config.h"
#include "menu.h"
#include "menuitems.h"
#include "device.h"
#ifdef ENABLE_SUSPEND
# ifdef SUSPEND_BY_PLAYER
#  include "dummy_player.h"
# endif
#endif
#include "media_player.h"
#include "equalizer.h"

//--------------------------- cMenuBrowseFiles -------------------------------

class cMenuBrowseFiles : public cOsdMenu 
{
  protected:
    char *m_CurrentDir;
    bool m_Images, m_Preview;
    char *m_ConfigLastDir;

    virtual bool ScanDir(const char *DirName);
    virtual eOSState Open(bool Parent=false);
    virtual eOSState Delete(void);
    virtual eOSState Info(void);
    virtual void Set(void);
    virtual void SetHelpButtons(void);
    cFileListItem *GetCurrent() { return (cFileListItem *)Get(Current()); }
    void StoreConfig(void);

  public:
    cMenuBrowseFiles(const char *title, bool images = false, bool preview = false);
    ~cMenuBrowseFiles();

    virtual eOSState ProcessKey(eKeys Key);
};

static char *ParentDir(const char *dir)
{
  char *result = strdup(dir);
  char *pt = strrchr(result, '/');
  if(pt)
    *(pt+1)=0;
  if(pt != result)
    *pt = 0;
  return result;
}

static char *LastDir(const char *dir)
{
  char *pt = strrchr(dir, '/');
  if(pt && pt[0] && pt[1])
    return strdup(pt+1);
  return NULL;
}

cMenuBrowseFiles::cMenuBrowseFiles(const char *title, bool images, bool preview) :
    cOsdMenu(title, 2, 4)
{
  m_CurrentDir = NULL;
  m_Images  = images;
  m_Preview = preview;
  m_ConfigLastDir = (!m_Images) ? xc.browse_files_dir : xc.browse_images_dir;
  Set();
}

cMenuBrowseFiles::~cMenuBrowseFiles()
{
  Setup.Save();
  free(m_CurrentDir);
}

void cMenuBrowseFiles::Set(void)
{
  Clear();

  if(!m_CurrentDir) 
    m_CurrentDir = strdup(m_ConfigLastDir);

  // find deepest accessible directory from path
  while(!ScanDir(m_CurrentDir) && strlen(m_CurrentDir) > 1) {
    char *n = ParentDir(m_CurrentDir);
    free(m_CurrentDir);
    m_CurrentDir = n;
  }

  // add link to parent folder
  if(strlen(m_CurrentDir) > 1)
    Add(new cFileListItem("..",true));

  Sort();

  SetCurrent(Get(Count()>1 && strlen(m_CurrentDir)>1 ? 1 : 0));

  // select last selected item

  char *lastParent = ParentDir(m_ConfigLastDir);
  if(!strncmp(m_CurrentDir,lastParent,strlen(m_CurrentDir))) {
    char *item = LastDir(m_ConfigLastDir);
    if(item) {
      for(cFileListItem *it = (cFileListItem*)First(); it; it = (cFileListItem*)Next(it))
	if(!strcmp(it->Name(),item))
	  SetCurrent(it);
      free(item);
    }
  }
  free(lastParent);

  strcpy(m_ConfigLastDir, m_CurrentDir);
  StoreConfig();

  SetHelpButtons();
}

void cMenuBrowseFiles::StoreConfig(void)
{
  if(!m_Images)
    cPluginManager::GetPlugin(PLUGIN_NAME_I18N)->SetupStore("BrowseFilesDir",  
							    xc.browse_files_dir);
  else    
    cPluginManager::GetPlugin(PLUGIN_NAME_I18N)->SetupStore("BrowseImagesDir", 
							    xc.browse_images_dir);
}

void cMenuBrowseFiles::SetHelpButtons(void)
{
  bool isDir = !GetCurrent() || GetCurrent()->IsDir(); 
  SetHelp(tr("Button$Select"), strlen(m_CurrentDir) > 1 ? "[..]" : NULL, 
	  isDir ? NULL : tr("Button$Delete"), isDir ? NULL : tr("Button$Info"));
  Display();
}

eOSState cMenuBrowseFiles::Delete(void)
{
  cFileListItem *it = GetCurrent();
  if(!it->IsDir()) {
    if (Interface->Confirm(tr("Delete recording?"))) {
      char *name = NULL;
      asprintf(&name, "%s/%s", m_CurrentDir, it->Name());
      if(!unlink(name)) {
        isyslog("file %s deleted", name);
	if(!m_Images) {
	  free(name);
	  name=NULL;
	  asprintf(&name, "%s/%s.resume", m_CurrentDir, it->Name());
	  unlink(name);
	}
        cOsdMenu::Del(Current());
	SetHelpButtons();
        Display();
      } else {
        Skins.Message(mtError, tr("Error while deleting recording!"));
        isyslog("Error deleting file %s", name);
      }
      free(name);
    }
  }
  return osContinue;
}

eOSState cMenuBrowseFiles::Open(bool Parent)
{
  if(!GetCurrent()) {
    return osContinue;
  }

  /* parent directory */
  if(Parent || !strcmp("..", GetCurrent()->Name())) {
    char *n = ParentDir(m_CurrentDir);
    free(m_CurrentDir);
    m_CurrentDir = n;
    Set();
    return osContinue;

  /* directory */
  } else if (GetCurrent()->IsDir()) {
    const char *d = GetCurrent()->Name();
    char *buffer = NULL;
    asprintf(&buffer, "%s/%s", m_CurrentDir, d);
    free(m_CurrentDir);
    m_CurrentDir = buffer;
    Set();
    return osContinue;

  /* regular file */
  } else {
    char *f = NULL;
    asprintf(&f, "%s/%s", m_CurrentDir, GetCurrent()->Name());
    strcpy(m_ConfigLastDir, f);
    StoreConfig();
    if(!m_Images) {
      /* video/audio */
      cControl::Launch(new cXinelibPlayerControl(f));
    } else {
      /* image */
      char **files = new char*[Count()+1];
      int i=0, index = 0;
      memset(files, 0, sizeof(char*)*(Count()+1));
      for(cFileListItem *it = (cFileListItem*)First(); it; it=(cFileListItem*)Next(it)) {
	if(it==Get(Current()))
	  index = i;
	if(!it->IsDir())
	  asprintf(&files[i++], "%s/%s", m_CurrentDir, it->Name());
      }
      cControl::Launch(new cXinelibImagesControl(files, index, i));
    }
    free(f);
    return osEnd;
  }
  return osContinue;
}

eOSState cMenuBrowseFiles::Info(void)
{
  if(GetCurrent() && !GetCurrent()->IsDir()) {
    char *cmd=NULL, buf[4096];
    asprintf(&cmd,"file '%s/%s'", m_CurrentDir, GetCurrent()->Name());
    FILE *f = popen(cmd, "r");
    free(cmd);
    if(f) {
      int n=0, ch;
      while((ch = fgetc(f)) != EOF && n<4000) 
	buf[n++] = ch;
      buf[n] = 0;
      if(n>0) {
        buf[n] = 0;
        strreplace(buf, ',', '\n');
	fclose(f);
        return AddSubMenu(new cMenuText(GetCurrent()->Name(), buf));
      }
      fclose(f);
    }
  }

  return osContinue;
}

static bool IsVideoFile(const char *fname)
{
  char *pos = strrchr(fname,'.');
  if(pos) {
    if(!strcasecmp(pos, ".avi") ||
       !strcasecmp(pos, ".mpv") ||
       !strcasecmp(pos, ".vob") || 
       !strcasecmp(pos, ".vdr") || 
       !strcasecmp(pos, ".mpg") ||
       !strcasecmp(pos, ".mpeg")|| 
       !strcasecmp(pos, ".mpa") || 
       !strcasecmp(pos, ".mp2") || 
       !strcasecmp(pos, ".mp3") || 
       !strcasecmp(pos, ".mp4") || 
       !strcasecmp(pos, ".asf") || 
       !strcasecmp(pos, ".ram"))
      return true;
  }
  return false;
}

static bool IsImageFile(const char *fname)
{
  char *pos = strrchr(fname,'.');
  if(pos) {
    if(!strcasecmp(pos, ".jpg") ||
       !strcasecmp(pos, ".jpeg") ||
       !strcasecmp(pos, ".gif") ||
       !strcasecmp(pos, ".tiff") || 
       !strcasecmp(pos, ".bmp") || 
       !strcasecmp(pos, ".png"))
      return true;
  }
  return false;
}

bool cMenuBrowseFiles::ScanDir(const char *DirName)
{
  DIR *d = opendir(DirName);
  if (d) {
    struct dirent *e;
    while ((e = readdir(d)) != NULL) {
      if (strcmp(e->d_name, ".") && strcmp(e->d_name, "..")) {
        char *buffer;
        asprintf(&buffer, "%s/%s", DirName, e->d_name);
        struct stat st;
        if (stat(buffer, &st) == 0) {

	  // check symlink destination
          if (S_ISLNK(st.st_mode)) {
	    free(buffer);
	    buffer = ReadLink(buffer);
	    if (!buffer)
	      continue;
	    if (stat(buffer, &st) != 0) {
	      free(buffer);
	      continue;
	    }
	  }

	  // folders
          if (S_ISDIR(st.st_mode)) {
	    if(m_Images)
	      Add(new cFileListItem(e->d_name,true));
	    else
	      Add(new cFileListItem(e->d_name,true,false,false));

          // regular files
          } else {
	    // video/audio
            if (!m_Images && IsVideoFile(buffer)) {
              bool resume=false, subs=false;
              free(buffer);
              buffer=NULL;
              asprintf(&buffer, "%s/%s.resume", DirName, e->d_name);
              if (stat(buffer, &st) == 0)
                resume=true;
              *strrchr(buffer,'.')=0;
              strcpy(strrchr(buffer,'.'), ".sub");
              if (stat(buffer, &st) == 0)
                subs=true;
              strcpy(strrchr(buffer,'.'), ".srt");
              if (stat(buffer, &st) == 0)
                subs=true;
              Add(new cFileListItem(e->d_name,false,resume,subs));

	    // images
            } else if(m_Images && IsImageFile(buffer)) {
              Add(new cFileListItem(e->d_name,false));
	    }
          }
        }
        free(buffer);
      }
    }
    closedir(d);
    return true;
  }
  return false;
}

eOSState cMenuBrowseFiles::ProcessKey(eKeys Key)
{
  eOSState state = cOsdMenu::ProcessKey(Key);

  if (state == osUnknown) {
     switch (Key) {
       case kPlay:   if(!GetCurrent()->IsDir()) return Open();
       case kOk:
       case kRed:    return Open();
       case kGreen:  return Open(true);
       case kYellow: return Delete();
       case kBlue:   return Info();
       default: break;
       }
     }

  if(!HasSubMenu())
    SetHelpButtons();

  return state;
}

//--------------------------- cMenuTestImages-------------------------------

#include <vdr/osdbase.h>

#define OSD_W (720-2)
#define OSD_H (576-2)
#define OSD_X (1)
#define OSD_Y (1)

//
// cTestGrayscale
//

class cTestGrayscale : public cOsdObject 
{
  private:
    cOsd *m_Osd;

  public:
    cTestGrayscale() { m_Osd = NULL; }
    virtual ~cTestGrayscale() { delete m_Osd; }

    virtual void Show();
    virtual eOSState ProcessKey(eKeys Key);
};

void cTestGrayscale::Show()
{
  tArea areas [] = { {      0, 0, OSD_W/2 - 1, OSD_H - 1, 8},
                     {OSD_W/2, 0,   OSD_W - 1, OSD_H - 1, 8}};
  int i;

  if(!m_Osd)
    m_Osd = cOsdProvider::NewOsd(OSD_X, OSD_Y);

  if(m_Osd) {
    if (m_Osd->CanHandleAreas(areas, sizeof(areas) / sizeof(tArea) ) == oeOk) {
      m_Osd->SetAreas(areas, sizeof(areas) / sizeof(tArea));
      m_Osd->Flush();

      // border
      m_Osd->DrawRectangle(0, 0, OSD_W - 1, OSD_H - 1, 0xff000000);
      m_Osd->DrawRectangle(1, 1, OSD_W - 2, OSD_H - 2, 0xff000000);

      // background
      m_Osd->DrawRectangle(2, 2, 2+103, OSD_H - 3, 0xffffffff); 
      m_Osd->DrawRectangle(OSD_W-2-103, 2, OSD_W-2, OSD_H - 3, 0xff000000); 

      for(i=0; i<0xff; i++)
	m_Osd->DrawRectangle(2+103+2*i, 2, 2+103+2*(i+1), OSD_H - 3, 
			     0xff000000|(i*0x00010101)/*=(i<<16)|(i<<8)|(i)*/);
      // line
      m_Osd->DrawRectangle(1, OSD_H/2-20, OSD_W - 2, OSD_H/2, 0xffffffff);
      m_Osd->DrawRectangle(1, OSD_H/2+1, OSD_W - 2, OSD_H/2+21, 0xff000000);

      // Cross
      for(int x=0; x<OSD_W;x++) {
	m_Osd->DrawPixel(x, x*OSD_H/OSD_W, 0x00000000);
	m_Osd->DrawPixel(x, OSD_H - 1 - x*OSD_H/OSD_W, 0x00000000);
      }

      // commit
      m_Osd->Flush();
    }

  }
}

eOSState cTestGrayscale::ProcessKey(eKeys key)
{
  char s[32];
  static int br = xc.brightness;
  static int co = xc.contrast;
  eOSState state = cOsdObject::ProcessKey(key);
  if (state == osUnknown) {
    switch (key & ~k_Repeat) {
      case kOk:
      case kBack:
	return osEnd;
      case kRight:
	br += 0xffff/1024*2;
      case kLeft:
	br -= 0xffff/1024;
	sprintf(s, "b %d", br);
	m_Osd->DrawText(400, 100, s, 0xff000000, 0xffffffff, cFont::GetFont(fontSml));
	cXinelibDevice::Instance().ConfigureVideo(xc.hue, xc.saturation, br, co);
	m_Osd->Flush();
	return osContinue;	
      case kUp:
	co += 0xffff/1024*2;
      case kDown:
	co -= 0xffff/1024;
	sprintf(s, "c %d", co);
	m_Osd->DrawText(400, 130, s, 0xff000000, 0xffffffff, cFont::GetFont(fontSml));
	cXinelibDevice::Instance().ConfigureVideo(xc.hue, xc.saturation, br, co);
	m_Osd->Flush();
	return osContinue;
    }
  }
  return state;
}


//
// cTestBitmap
//

class cTestBitmap : public cOsdObject 
{
  private:
    cOsd *m_Osd;
    int bpp;

  public:
    cTestBitmap(int _bpp = 1) { 
      m_Osd = NULL; 
      if(_bpp<1) _bpp = 1; 
      if(_bpp>6) _bpp = 6; 
      bpp = 1<<_bpp; 
    }
    virtual ~cTestBitmap() { delete m_Osd; }

    virtual void Show();
    virtual eOSState ProcessKey(eKeys Key);
};

void cTestBitmap::Show()
{
  tArea areas [] = {{ 0, 0, OSD_W - 1, OSD_H - 1, 8}};
  int x, y, bit = 0;

  if(!m_Osd) {
    m_Osd = cOsdProvider::NewOsd(OSD_X, OSD_Y);

    if(m_Osd) {
      if (m_Osd->CanHandleAreas(areas, sizeof(areas) / sizeof(tArea) ) == oeOk) {
	m_Osd->SetAreas(areas, sizeof(areas) / sizeof(tArea));
	m_Osd->Flush();
      }
    }      
  }
   
  if(m_Osd) {
    for(x=0; x<OSD_W; x+=bpp) {
      bit = (x/bpp) & 1;
      for(y=0; y<OSD_H; y+=bpp) {
	m_Osd->DrawRectangle(x, y, x+bpp, y+bpp, bit?0xffffffff:0xff000000);
	bit = !bit;
      }
    }
    // commit
    m_Osd->Flush();
  }
}

eOSState cTestBitmap::ProcessKey(eKeys key)
{
  eOSState state = cOsdObject::ProcessKey(key);

  if (state == osUnknown) {
    switch (key & ~k_Repeat) {
      case kOk:
      case kBack:
	return osEnd;
      case kRight:
	bpp = (bpp<64) ? (bpp<<1) : 1;
	Show();
	return osContinue;
      case kLeft:
	bpp = (bpp>1) ? (bpp>>1) : 64;
	Show();
	return osContinue;	
      default:
	break;
    }
  }
  return state;
}

//----------------------------- cMenuXinelib ---------------------------------

static cOsdItem *NewTitle(const char *s)
{
  char str[128];
  cOsdItem *tmp;
  sprintf(str,"----- %s -----", tr(s));
  tmp = new cOsdItem(str);
  tmp->SetSelectable(false);
  return tmp;
}


const char *decoderState[] = {"running", "paused", NULL};
extern cOsdObject *g_PendingMenuAction;

cMenuXinelib::cMenuXinelib()
{
  field_order = xc.field_order;
#ifdef ENABLE_SUSPEND
  suspend = cXinelibDevice::Instance().IsSuspended();
#endif
  compression = xc.audio_compression;
  headphone = xc.headphone;
  autocrop = xc.autocrop;

  Add(new cOsdItem(tr("Play file >>"), osUser1));
  Add(new cOsdItem(tr("View images >>"), osUser2));
#if 0
  Add(new cOsdItem(tr("Play remote DVD >>"), osUser4));
#endif
#ifdef ENABLE_TEST_POSTPLUGINS
#warning Experimental post plugins enabled !
  Add(ctrl_headphone = new cMenuEditBoolItem(tr("Headphone audio mode"), 
					     &headphone));
  Add(ctrl_autocrop = new cMenuEditBoolItem(tr("Remove letterbox (4:3 -> 16:9)"), 
					     &autocrop));
#else
  ctrl_headphone = NULL;
  ctrl_autocrop = NULL;
#endif

#ifdef HAVE_XV_FIELD_ORDER
  Add(video_ctrl_interlace_order = new cMenuEditStraI18nItem(tr("Interlaced Field Order"), &field_order, 2, xc.s_fieldOrder));
#endif
#ifdef ENABLE_SUSPEND
  Add(decoder_ctrl_suspend = new cMenuEditStraI18nItem(tr("Decoder state"), &suspend, 2, decoderState));
#endif
  Add(audio_ctrl_compress = new cMenuEditTypedIntItem(tr("Audio Compression"),"%", &compression, 100, 500, tr("Off")));

  Add(new cOsdItem(tr("Audio Equalizer >>"), osUser3));

  switch(xc.main_menu_mode) {
    case ShowFiles:  AddSubMenu(new cMenuBrowseFiles(tr("Play file"))); break;
    case ShowImages: AddSubMenu(new cMenuBrowseFiles(tr("Images"),true,true)); break;
    default: break;
  }

  /* #warning should be separate plugin ? fbconfig / x11config */
  Add(NewTitle("Test Images"));
  char buf[128];
  Add(new cOsdItem(tr("Grayscale"), osUser8));
  sprintf(buf, "%s 1bit", tr("Bitmap"));
  Add(new cOsdItem(buf, osUser9));
  sprintf(buf, "%s 4bit", tr("Bitmap"));
  Add(new cOsdItem(buf, osUser10));

  xc.main_menu_mode = ShowMenu;
}

cMenuXinelib::~cMenuXinelib()
{
#ifdef HAVE_XV_FIELD_ORDER
  if(xc.field_order != field_order )
    cXinelibDevice::Instance().ConfigureWindow(xc.fullscreen, xc.width, xc.height, xc.modeswitch, xc.modeline, xc.display_aspect, xc.scale_video, xc.field_order);
#endif

  if(xc.audio_compression != compression)
    cXinelibDevice::Instance().ConfigurePostprocessing(xc.deinterlace_method, xc.audio_delay, xc.audio_compression, xc.audio_equalizer, xc.audio_surround);

  if(xc.headphone != headphone)
    cXinelibDevice::Instance().ConfigurePostprocessing("headphone", 
						       xc.headphone ? true : false);

  if(xc.autocrop != autocrop)
    cXinelibDevice::Instance().ConfigurePostprocessing("autocrop", 
						       xc.autocrop ? true : false);
}

eOSState cMenuXinelib::ProcessKey(eKeys Key)
{
  cOsdItem *item = Get(Current());

  eOSState state = cMenuSetupPage::ProcessKey(Key);

  if(HasSubMenu())
    return state;

  switch(state) {
    case osUser1:
      AddSubMenu(new cMenuBrowseFiles(tr("Play file")));
      return osUnknown;
    case osUser2:
      AddSubMenu(new cMenuBrowseFiles(tr("Images"), true, true));
      return osContinue;
    case osUser3:
      if(!g_PendingMenuAction) {
	g_PendingMenuAction = new cEqualizer();
	return osPlugin;
      }
      state = osContinue;
    case osUser4:
      cControl::Launch(new cXinelibPlayerControl("dvd://"));
      return osEnd;
    case osUser8:
      if(!g_PendingMenuAction) {
	g_PendingMenuAction = new cTestGrayscale();
	return osPlugin;
      }
      return osContinue;
    case osUser9:
      if(!g_PendingMenuAction) {
	g_PendingMenuAction = new cTestBitmap(1);
	return osPlugin;
      }
      return osContinue;
    case osUser10:
      if(!g_PendingMenuAction) {
	g_PendingMenuAction = new cTestBitmap(4);
	return osPlugin;
      }
      return osContinue;
    default: ;
  }

  if(Key==kLeft || Key==kRight) {
    if(item == audio_ctrl_compress)
      cXinelibDevice::Instance().ConfigurePostprocessing(xc.deinterlace_method, xc.audio_delay, compression, xc.audio_equalizer, xc.audio_surround);
    else if(item == ctrl_headphone)
      cXinelibDevice::Instance().ConfigurePostprocessing("headphone", headphone?true:false);    
    else if(item == ctrl_autocrop)
      cXinelibDevice::Instance().ConfigurePostprocessing("autocrop", autocrop?true:false);
#ifdef ENABLE_SUSPEND
    else if(decoder_ctrl_suspend && item == decoder_ctrl_suspend) {
      cXinelibDevice::Instance().Suspend(suspend);
# ifdef SUSPEND_BY_PLAYER
      if(suspend && !cDummyPlayerControl::IsOpen()) {
        cControl::Launch(new cDummyPlayerControl);
      } else {
        cDummyPlayerControl::Close();
      }
# endif
    }
#endif
#ifdef HAVE_XV_FIELD_ORDER
    else if(video_ctrl_interlace_order && item == video_ctrl_interlace_order)
      cXinelibDevice::Instance().ConfigureWindow(xc.fullscreen, xc.width, xc.height, xc.modeswitch, xc.modeline, xc.display_aspect, xc.scale_video, field_order);
#endif
  }
  
  return state;
}
  
void cMenuXinelib::Store(void)
{
#ifdef HAVE_XV_FIELD_ORDER
  xc.field_order = field_order;
#endif
  xc.audio_compression = compression;
  xc.autocrop = autocrop;
  xc.headphone = headphone;
}

