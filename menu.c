/*
 * menu.c: Main Menu
 *
 * See the main source file 'xineliboutput.c' for copyright information and
 * how to reach the author.
 *
 * $Id: menu.c,v 1.15 2006-09-19 06:06:35 phintuka Exp $
 *
 */

#include <dirent.h>

#include <vdr/config.h>
#include <vdr/i18n.h>
#include <vdr/interface.h>
#include <vdr/menu.h>
#include <vdr/plugin.h>

#include "logdefs.h"
#include "config.h"
#include "menu.h"
#include "menuitems.h"
#include "device.h"
#include "media_player.h"
#include "equalizer.h"

#define ISNUMBERKEY(k) (RAWKEY(k) >= k0 && RAWKEY(k) <= k9)

//--------------------------- cMenuBrowseFiles -------------------------------

class cMenuBrowseFiles : public cOsdMenu 
{
  protected:
    char         *m_CurrentDir;
    eMainMenuMode m_Mode;
    bool          m_Preview;
    char         *m_ConfigLastDir;

    virtual bool ScanDir(const char *DirName);
    virtual eOSState Open(bool ForceOpen = false, bool Parent = false);
    virtual eOSState Delete(void);
    virtual eOSState Info(void);
    virtual void Set(void);
    virtual void SetHelpButtons(void);
    cFileListItem *GetCurrent() { return (cFileListItem *)Get(Current()); }
    void StoreConfig(void);
    char *GetLastDir(void);

  public:
    cMenuBrowseFiles(const char *title, eMainMenuMode mode = ShowFiles, bool preview = false);
    ~cMenuBrowseFiles();

    virtual eOSState ProcessKey(eKeys Key);
};

static char *ParentDir(const char *dir)
{
  char *result = strdup(dir);
  char *pt = strrchr(result, '/');
  if(pt) {
    *(pt+1)=0;
    if(pt != result)
      *pt = 0;
  }
  return result;
}

static char *LastDir(const char *dir)
{
  char *pt = strrchr(dir, '/');
  if(pt && pt[0] && pt[1])
    return strdup(pt+1);
  return NULL;
}

cMenuBrowseFiles::cMenuBrowseFiles(const char *title, eMainMenuMode mode, bool preview) :
    cOsdMenu(title, 2, 4)
{
  m_CurrentDir = NULL;
  m_Mode    = mode;
  m_Preview = preview;
  m_ConfigLastDir = GetLastDir();
  Set();
}

cMenuBrowseFiles::~cMenuBrowseFiles()
{
  Setup.Save();
  free(m_CurrentDir);
}

char *cMenuBrowseFiles::GetLastDir(void)
{
  switch(m_Mode) {
    case ShowMusic:  return xc.browse_music_dir;
    case ShowImages: return xc.browse_images_dir;
    default:
    case ShowFiles:  return xc.browse_files_dir;
  }
  return xc.browse_files_dir;
}

void cMenuBrowseFiles::Set(void)
{
  Clear();

  if(!m_CurrentDir) 
    m_CurrentDir = strdup(m_ConfigLastDir);

  if(m_CurrentDir[0] != '/') {
    free(m_CurrentDir);
    m_CurrentDir = strdup("/video");
  }

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
  cPluginManager::GetPlugin(PLUGIN_NAME_I18N)->SetupStore("BrowseMusicDir",  
							  xc.browse_music_dir);
  cPluginManager::GetPlugin(PLUGIN_NAME_I18N)->SetupStore("BrowseFilesDir",  
							  xc.browse_files_dir);
  cPluginManager::GetPlugin(PLUGIN_NAME_I18N)->SetupStore("BrowseImagesDir", 
							  xc.browse_images_dir);
}

void cMenuBrowseFiles::SetHelpButtons(void)
{
  bool isDir = !GetCurrent() || GetCurrent()->IsDir();
  bool isDvd = GetCurrent() && GetCurrent()->IsDvd();
  SetHelp(isDvd ? tr("Button$Open") : tr("Button$Play"),
	  strlen(m_CurrentDir) > 1 ? "[..]" : NULL,
	  (isDir && !isDvd) ? NULL : tr("Button$Delete"),
	  isDir ? NULL : tr("Button$Info"));
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
	if(m_Mode != ShowImages) {
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

eOSState cMenuBrowseFiles::Open(bool ForceOpen, bool Parent)
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

    if(!ForceOpen && GetCurrent()->IsDvd()) {
      /* play dvd */
      char *f = NULL;
      asprintf(&f, "dvd:%s/%s", m_CurrentDir, GetCurrent()->Name());
      cControl::Shutdown();
      cControl::Launch(new cXinelibDvdPlayerControl(f));
      free(f);
      return osEnd;
    }
    if(ForceOpen && GetCurrent()->IsDir()) {
      /* play all files */ 
      if(m_Mode != ShowImages) {
	char *f = NULL;
	asprintf(&f, "%s/%s/", m_CurrentDir, GetCurrent()->Name());
	cControl::Shutdown();
	cControl::Launch(new cXinelibPlayerControl(m_Mode, f));
	free(f);
	return osEnd;
      } else {
	// TODO: show all images
      }
    }
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
    if(m_Mode != ShowImages) {
      /* video/audio */
      cControl::Shutdown();
      cControl::Launch(new cXinelibPlayerControl(m_Mode, f));
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
      cControl::Shutdown();
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

bool cMenuBrowseFiles::ScanDir(const char *DirName)
{
  DIR *d = opendir(DirName);
  if (d) {
    struct dirent *e;
    while ((e = readdir(d)) != NULL) {
      if (strcmp(e->d_name, ".") && strcmp(e->d_name, "..")) {
        char *buffer = NULL;
        asprintf(&buffer, "%s/%s", DirName, e->d_name);
        struct stat st;
        if (stat(buffer, &st) == 0) {

	  // check symlink destination
          if (S_ISLNK(st.st_mode)) {
	    char *old = buffer;
	    buffer = ReadLink(buffer);
	    free(old);
	    if (!buffer)
	      continue;
	    if (stat(buffer, &st) != 0) {
	      free(buffer);
	      continue;
	    }
	  }

	  // folders
          if (S_ISDIR(st.st_mode)) {
	    if(m_Mode == ShowImages)
	      Add(new cFileListItem(e->d_name, true));
	    else {
	      // check if DVD
              bool dvd = false;
              free(buffer);
              buffer = NULL;
              asprintf(&buffer, "%s/%s/VIDEO_TS/VIDEO_TS.IFO", DirName, e->d_name);
              if (stat(buffer, &st) == 0)
                dvd = true;
	      else {
		free(buffer);
		buffer = NULL;
		asprintf(&buffer, "%s/%s/video_ts/video_ts.ifo", DirName, e->d_name);
		if (stat(buffer, &st) == 0)
		  dvd = true;
	      }
	      Add(new cFileListItem(e->d_name, true, false, false, dvd));
	    }

          // regular files
          } else {
	    // video/audio
            if (m_Mode != ShowImages && xc.IsVideoFile(buffer)) {
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
            } else if(m_Mode == ShowImages && xc.IsImageFile(buffer)) {
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
       case kPlay:   
       case kOk:     return Open();
       case kRed:    return Open(true);
       case kGreen:  return Open(false, true);
       case kYellow: return Delete();
       case kBlue:   return Info();
       default: break;
       }
     }

  if(!HasSubMenu())
    SetHelpButtons();

  return state;
}


//-------------------------- cDvdSpuTrackSelect ------------------------------

class cDvdSpuTrackSelect : public cOsdMenu
{
  public:
    cDvdSpuTrackSelect(void);
    virtual eOSState ProcessKey(eKeys Key);
};

cDvdSpuTrackSelect::cDvdSpuTrackSelect(void) : 
      cOsdMenu(tr("Select DVD SPU Track")) 
{
  int count = cXinelibDevice::Instance().NumDvdSpuTracks();
  int id = 0;
  int current = cXinelibDevice::Instance().GetCurrentDvdSpuTrack();
  Add(new cOsdItem("None", osUser1));
  while(count && id < 64) {
    if(cXinelibDevice::Instance().HasDvdSpuTrack(id)) {
      char name[64];
      sprintf(name, "Track %d", id);
      Add(new cOsdItem(name, osUser1));
      count--;
      if(id == current)
	SetCurrent(Get(Count()-1));
    }
    id++;
  }
}

eOSState cDvdSpuTrackSelect::ProcessKey(eKeys Key)
{
  eOSState state = cOsdMenu::ProcessKey(Key);

  switch(state) {
    case osUser1: 
      {
	const char *txt = Get(Current())->Text();
	int id = -1; /* -> DVD SPU off */
	sscanf(txt, "Track %d", &id);
	cXinelibDevice::Instance().SetCurrentDvdSpuTrack(id);
	AddSubMenu(new cMenuBrowseFiles(tr("Play file")));
	return osEnd;
      }
    default: break;
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
  compression = xc.audio_compression;
  headphone = xc.headphone;
  autocrop = xc.autocrop;
  overscan = xc.overscan;

  novideo = cXinelibDevice::Instance().GetPlayMode() == pmAudioOnlyBlack ? 1 : 0;

  Add(NewTitle("Media"));
  Add(new cOsdItem(tr("Play file >>"), osUser1));
  Add(new cOsdItem(tr("Play music >>"), osUser2));
  Add(new cOsdItem(tr("View images >>"), osUser3));
  if(xc.remote_mode)
    Add(new cOsdItem(tr("Play remote DVD >>"), osUser4));
  else
    Add(new cOsdItem(tr("Play DVD disc >>"), osUser4));
  if(cXinelibDevice::Instance().NumDvdSpuTracks() > 0)
    Add(new cOsdItem(tr("  Select DVD SPU track >>"), osUser5));

  Add(NewTitle("Video settings"));
  Add(ctrl_novideo = new cMenuEditBoolItem(tr("Play only audio"), 
					   &novideo));
  Add(ctrl_autocrop = new cMenuEditBoolItem(tr("Crop letterbox 4:3 to 16:9"), 
					    &autocrop));
  Add(ctrl_overscan = new cMenuEditTypedIntItem(tr("Overscan (crop image borders)"), "%",
						&overscan, 0, 10,
						tr("Off")));
#ifdef HAVE_XV_FIELD_ORDER
  Add(video_ctrl_interlace_order = new cMenuEditStraI18nItem(tr("Interlaced Field Order"), 
							     &field_order, 2, xc.s_fieldOrder));
#endif

  Add(NewTitle("Audio settings"));
#ifdef ENABLE_TEST_POSTPLUGINS
  Add(ctrl_headphone = new cMenuEditBoolItem(tr("Headphone audio mode"), 
					     &headphone));
#else
  ctrl_headphone = NULL;
#endif

  Add(audio_ctrl_compress = new cMenuEditTypedIntItem(tr("Audio Compression"),"%", &compression, 100, 500, tr("Off")));

  Add(new cOsdItem(tr("Audio equalizer >>"), osUser7));

  switch(xc.main_menu_mode) {
    case ShowFiles:  AddSubMenu(new cMenuBrowseFiles(tr("Play file"),  ShowFiles)); break;
    case ShowMusic:  AddSubMenu(new cMenuBrowseFiles(tr("Play music"), ShowMusic)); break;
    case ShowImages: AddSubMenu(new cMenuBrowseFiles(tr("Images"),     ShowImages, true)); break;
    default: break;
  }

  xc.main_menu_mode = ShowMenu;
}

cMenuXinelib::~cMenuXinelib()
{
#ifdef HAVE_XV_FIELD_ORDER
  if(xc.field_order != field_order )
    cXinelibDevice::Instance().ConfigureWindow(xc.fullscreen, xc.width, xc.height, 
					       xc.modeswitch, xc.modeline, xc.display_aspect, 
					       xc.scale_video, xc.field_order);
#endif

  if(xc.audio_compression != compression)
    cXinelibDevice::Instance().ConfigurePostprocessing(xc.deinterlace_method, xc.audio_delay, 
						       xc.audio_compression, xc.audio_equalizer, 
						       xc.audio_surround, xc.speaker_type);

  if(xc.overscan != overscan)
    cXinelibDevice::Instance().ConfigureVideo(xc.hue, xc.saturation, xc.brightness,
					      xc.contrast, xc.overscan);

  if(xc.headphone != headphone)
    cXinelibDevice::Instance().ConfigurePostprocessing("headphone", 
						       xc.headphone ? true : false);

  if(xc.autocrop != autocrop)
    cXinelibDevice::Instance().ConfigurePostprocessing("autocrop", 
						       xc.autocrop ? true : false,
						       xc.AutocropOptions());

  int dev_novideo = cXinelibDevice::Instance().GetPlayMode() == pmAudioOnlyBlack ? 1 : 0;
  if(dev_novideo != novideo) 
    cXinelibDevice::Instance().SetPlayMode(novideo ? pmAudioOnlyBlack : pmNone);
}

eOSState cMenuXinelib::ProcessKey(eKeys Key)
{
  cOsdItem *item = Get(Current());

  eOSState state = cMenuSetupPage::ProcessKey(Key);

  if(HasSubMenu())
    return state;

  switch(state) {
    case osUser1:
      AddSubMenu(new cMenuBrowseFiles(tr("Play file"), ShowFiles));
      return osUnknown;
    case osUser2:
      AddSubMenu(new cMenuBrowseFiles(tr("Play music"), ShowMusic));
      return osUnknown;
    case osUser3:
      AddSubMenu(new cMenuBrowseFiles(tr("Images"), ShowImages, true));
      return osContinue;
    case osUser4:
      cControl::Shutdown();
      cControl::Launch(new cXinelibDvdPlayerControl("dvd:/"));
      return osEnd;
    case osUser5:
      AddSubMenu(new cDvdSpuTrackSelect());
      return osContinue;
    case osUser7:
      if(!g_PendingMenuAction) {
	g_PendingMenuAction = new cEqualizer();
	return osPlugin;
      }
      return osContinue;
    default: ;
  }

  Key = NORMALKEY(Key);

  if(Key==kLeft || Key==kRight || ISNUMBERKEY(Key)) {
    if(item == audio_ctrl_compress)
      cXinelibDevice::Instance().ConfigurePostprocessing(xc.deinterlace_method, xc.audio_delay, 
							 compression, xc.audio_equalizer, 
							 xc.audio_surround, xc.speaker_type);
    else if(item == ctrl_overscan)
      cXinelibDevice::Instance().ConfigureVideo(xc.hue, xc.saturation, xc.brightness,
                                                xc.contrast, overscan);
  }
  if(Key==kLeft || Key==kRight) {
    if(item == ctrl_headphone)
      cXinelibDevice::Instance().ConfigurePostprocessing("headphone", headphone?true:false);    
    else if(item == ctrl_autocrop)
      cXinelibDevice::Instance().ConfigurePostprocessing("autocrop", autocrop?true:false,
							 xc.AutocropOptions());
    else if(item == ctrl_novideo)
      cXinelibDevice::Instance().SetPlayMode(novideo ? pmAudioOnlyBlack : pmNone);
#ifdef HAVE_XV_FIELD_ORDER
    else if(video_ctrl_interlace_order && item == video_ctrl_interlace_order)
      cXinelibDevice::Instance().ConfigureWindow(xc.fullscreen, xc.width, xc.height, 
						 xc.modeswitch, xc.modeline, 
						 xc.display_aspect, xc.scale_video, 
						 field_order);
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
  xc.overscan = overscan;
  xc.headphone = headphone;
}

