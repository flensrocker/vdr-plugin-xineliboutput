/*
 * menu.c: Main Menu
 *
 * See the main source file 'xineliboutput.c' for copyright information and
 * how to reach the author.
 *
 * $Id: menu.c,v 1.27 2006-12-24 09:43:59 phintuka Exp $
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

#ifndef HOTKEY_START
# define HOTKEY_START        kRed

# define HOTKEY_DVD          k0    /* */
# define HOTKEY_DVD_TRACK1   k1    /* */
# define HOTKEY_DVD_SPU      k2    /* */

# define HOTKEY_NEXT_ASPECT  k3    /* auto, 4:3, 16:9 */
# define HOTKEY_TOGGLE_CROP  k4    /* off, force, auto */
# define HOTKEY_UPMIX        k5    /* off, on */
# define HOTKEY_DOWNMIX      k6    /* off, on */
# define HOTKEY_DEINTERLACE  k7    /* off, on */
# define HOTKEY_LOCAL_FE     k8    /* off, on */

# define HOTKEY_PLAYLIST     k9    /* Start replaying playlist or file pointed by
				      symlink $(CONFDIR)/plugins/xineliboutput/default_playlist */
#endif


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
  cPluginManager::GetPlugin(PLUGIN_NAME_I18N)->SetupStore("Media.BrowseMusicDir",  
							  xc.browse_music_dir);
  cPluginManager::GetPlugin(PLUGIN_NAME_I18N)->SetupStore("Media.BrowseFilesDir",  
							  xc.browse_files_dir);
  cPluginManager::GetPlugin(PLUGIN_NAME_I18N)->SetupStore("Media.BrowseImagesDir", 
							  xc.browse_images_dir);
#if 1
  cPluginManager::GetPlugin(PLUGIN_NAME_I18N)->SetupStore("Media.CacheImplicitPlaylists", 
							  xc.cache_implicit_playlists);
  cPluginManager::GetPlugin(PLUGIN_NAME_I18N)->SetupStore("Media.EnableID3Scanner", 
							  xc.enable_id3_scanner);
#endif
}

void cMenuBrowseFiles::SetHelpButtons(void)
{
  bool isDir = !GetCurrent() || GetCurrent()->IsDir();
  bool isDvd = GetCurrent() && GetCurrent()->IsDvd();
  SetHelp((isDir && isDvd) ? tr("Button$Open") : tr("Button$Play"),
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
    while(buffer[0] == '/' && buffer[1] == '/')
      strcpy(buffer, buffer+1);
    free(m_CurrentDir);
    m_CurrentDir = buffer;
    Set();
    return osContinue;
    
  /* regular file */
  } else {
    char *f = NULL;
    asprintf(&f, "%s%s/%s", 
	     GetCurrent()->IsDvd() ? "dvd:" : "",
	     m_CurrentDir, GetCurrent()->Name());
    strcpy(m_ConfigLastDir, f);
    StoreConfig();

    if(m_Mode != ShowImages) {
      /* video/audio */
      cControl::Shutdown();
      cControl::Launch(GetCurrent()->IsDvd()
		       ? new cXinelibDvdPlayerControl(f)
		       : new cXinelibPlayerControl(m_Mode, f));
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
	cString buffer = cString::sprintf("%s/%s", DirName, e->d_name);
        struct stat st;
        if (stat(buffer, &st) == 0) {

	  // check symlink destination
          if (S_ISLNK(st.st_mode)) {
	    buffer = ReadLink(buffer);
	    if (!*buffer || stat(buffer, &st))
	      continue;
	  }

	  // folders
          if (S_ISDIR(st.st_mode)) {
	    if(m_Mode == ShowImages || m_Mode == ShowMusic)
	      Add(new cFileListItem(e->d_name, true));
	    else {
	      // check if DVD
              bool dvd = false;
	      buffer = cString::sprintf("%s/%s/VIDEO_TS/VIDEO_TS.IFO", DirName, e->d_name);
              if (stat(buffer, &st) == 0)
                dvd = true;
	      else {
		buffer = cString::sprintf("%s/%s/video_ts/video_ts.ifo", DirName, e->d_name);
		if (stat(buffer, &st) == 0)
		  dvd = true;
	      }
	      Add(new cFileListItem(e->d_name, true, false, false, dvd));
	    }

          // regular files
          } else if(e->d_name[0] != '.') {

	    // audio
	    if (m_Mode == ShowMusic && xc.IsAudioFile(buffer)) {
	      Add(new cFileListItem(e->d_name, false));

	    // images
            } else if(m_Mode == ShowImages && xc.IsImageFile(buffer)) {
	      Add(new cFileListItem(e->d_name, false));

	    // video
	    } else if (m_Mode == ShowFiles && xc.IsVideoFile(buffer)) {
	      bool resume = false, subs = false, dvd = false;
	      char *pos = strrchr(e->d_name, '.');
	      
	      //free(buffer);
	      //buffer = NULL;

	      if(pos) {
		// .iso image -> dvd
		if(pos && !strcasecmp(pos, ".iso"))
		  dvd = true;

		// separate subtitles ?
		strcpy(strrchr(buffer,'.'), ".sub");
		if (stat(buffer, &st) == 0)
		  subs = true;
		strcpy(strrchr(buffer,'.'), ".srt");
		if (stat(buffer, &st) == 0)
		  subs = true;
		strcpy(strrchr(buffer,'.'), ".txt");
		if (stat(buffer, &st) == 0)
		  subs = true;
		strcpy(strrchr(buffer,'.'), ".ssa");
		if (stat(buffer, &st) == 0)
		  subs = true;
	      }

	      // resume file ?
	      buffer = cString::sprintf("%s/%s.resume", DirName, e->d_name);
	      if (stat(buffer, &st) == 0)
		resume = true;

	      Add(new cFileListItem(e->d_name, false, resume, subs, dvd));
	    }
          }
        }
        //free(buffer);
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

/* #warning TODO: use SelectAudioTrack skin display */
cDvdSpuTrackSelect::cDvdSpuTrackSelect(void) : 
      cOsdMenu(tr("Select DVD SPU Track")) 
{
  int count = cXinelibDevice::Instance().NumDvdSpuTracks();
  int id = 0;
  int current = cXinelibDevice::Instance().GetCurrentDvdSpuTrack();
  Add(new cOsdItem("None", osUser1));
  while(count && id < 64) {
    const tTrackId *track = cXinelibDevice::Instance().GetDvdSpuTrack(id);
    if(track) {
      char name[64];
      if(track->language[0])
	sprintf(name, "Track %d: %s", id, track->language);
      else
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

#include "tools/display_message.h"

static cOsdItem *NewTitle(const char *s)
{
  char str[128];
  cOsdItem *tmp;
  sprintf(str,"----- %s -----", s);
  tmp = new cOsdItem(str);
  tmp->SetSelectable(false);
  return tmp;
}


extern cOsdObject *g_PendingMenuAction;

time_t cMenuXinelib::g_LastHotkeyTime = 0;
eKeys  cMenuXinelib::g_LastHotkey = kNone;

cMenuXinelib::cMenuXinelib()
{
  field_order = xc.field_order;
  compression = xc.audio_compression;
  headphone = xc.headphone;
  autocrop = xc.autocrop;
  overscan = xc.overscan;

  hotkey_state = hkInit;

  novideo = cXinelibDevice::Instance().GetPlayMode() == pmAudioOnlyBlack ? 1 : 0;

  Add(NewTitle(tr("Media")));
  Add(new cOsdItem(tr("Play file >>"), osUser1));
  Add(new cOsdItem(tr("Play music >>"), osUser2));
  Add(new cOsdItem(tr("View images >>"), osUser3));
  if(xc.remote_mode)
    Add(new cOsdItem(tr("Play remote DVD >>"), osUser4));
  else
    Add(new cOsdItem(tr("Play DVD disc >>"), osUser4));
  if(xc.remote_mode)
    Add(new cOsdItem(tr("Play remote CD >>"), osUser6));
  else
    Add(new cOsdItem(tr("Play audio CD >>"), osUser6));
  if(cXinelibDevice::Instance().NumDvdSpuTracks() > 0)
    Add(new cOsdItem(tr("  Select DVD SPU track >>"), osUser5));

  Add(NewTitle(tr("Video settings")));
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

  Add(NewTitle(tr("Audio settings")));
#ifdef ENABLE_TEST_POSTPLUGINS
  Add(ctrl_headphone = new cMenuEditBoolItem(tr("Headphone audio mode"), 
					     &headphone));
#else
  ctrl_headphone = NULL;
#endif

  Add(audio_ctrl_compress = new cMenuEditTypedIntItem(tr("Audio Compression"),"%", 
						      &compression, 100, 500, NULL, tr("Off")));

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
  /* Hot key support */
  if(hotkey_state == hkInit && Key == kNone)
    return osContinue;
  if(hotkey_state == hkInit && Key == HOTKEY_START) {
    hotkey_state = hkSeen;
    return osContinue;
  } else if(hotkey_state == hkSeen && Key != kNone) {
    hotkey_state = hkNone;
    return ProcessHotkey(Key);
  }
  hotkey_state = hkNone;
    
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
    case osUser6:
      cControl::Shutdown();
      cControl::Launch(new cXinelibPlayerControl("cdda:/"));
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

#if APIVERSNUM < 10404
#  warning Using hotkeys may segfault with VDR version < 1.4.3-2
#endif

eOSState cMenuXinelib::ProcessHotkey(eKeys Key)
{
  eOSState NewState = osEnd;
  char    *Message  = NULL;
  time_t   now      = time(NULL);
  bool     OnlyInfo = ((g_LastHotkeyTime < now-3) || g_LastHotkey != Key);

  switch(Key) {
    case HOTKEY_DVD:
      cControl::Shutdown();
      cControl::Launch(new cXinelibDvdPlayerControl("dvd:/"));
      break;

    case HOTKEY_DVD_TRACK1:
      cControl::Shutdown();
      cControl::Launch(new cXinelibDvdPlayerControl("dvd:/1"));
      break;

    case HOTKEY_DVD_SPU:
      /* use audio track display menu ? */
      {
	int count = cXinelibDevice::Instance().NumDvdSpuTracks();
	int current = cXinelibDevice::Instance().GetCurrentDvdSpuTrack();
	if(!OnlyInfo) {
	  current++;
	  if(current == count)
	    current = -1;
	  cXinelibDevice::Instance().SetCurrentDvdSpuTrack(current);
	}
	const char *lang = cXinelibDevice::Instance().GetDvdSpuLang(current); 
	if(current == -1) lang = "default";
	if(lang && lang[0])
	  asprintf(&Message, "%s %s %s (%d)", tr("DVD SPU Track"), 
		   OnlyInfo ? ":" : "->",
		   lang, current);
	else
	  asprintf(&Message, "%s %s %d", tr("DVD SPU Track"), 
		   OnlyInfo ? ":" : "->", 
		   current);
      }
      break;

    case HOTKEY_LOCAL_FE:
      /* off, on */
      {
	int local_frontend = strstra(xc.local_frontend, xc.s_frontends, 0);
	if(!OnlyInfo) {
	  local_frontend++;
	  if(local_frontend >= FRONTEND_count)
	    local_frontend = 0;
	  strcpy(xc.local_frontend, xc.s_frontends[local_frontend]);
	  cXinelibDevice::Instance().ConfigureWindow(
	      xc.fullscreen, xc.width, xc.height, xc.modeswitch, xc.modeline, 
	      xc.display_aspect, xc.scale_video, xc.field_order);
	}
	asprintf(&Message, "%s %s %s", tr("Local Frontend"), 
		 OnlyInfo ? ":" : "->", 
		 xc.s_frontendNames[local_frontend]);
      }
      break;

    case HOTKEY_NEXT_ASPECT:
      /* auto, 4:3, 16:9, ... */
      if(!OnlyInfo) {
	xc.display_aspect = (xc.display_aspect < ASPECT_count-1) ? xc.display_aspect+1 : 0;
	cXinelibDevice::Instance().ConfigureWindow(xc.fullscreen, xc.width, xc.height, 
						   xc.modeswitch, xc.modeline, xc.display_aspect, 
						   xc.scale_video, xc.field_order);      
      }
      asprintf(&Message, "%s %s %s", tr("Aspect ratio"), 
	       OnlyInfo ? ":" : "->",
	       tr(xc.s_aspects[xc.display_aspect]));
      break;

    case HOTKEY_TOGGLE_CROP:    
      /* off, force, auto */
      if(!OnlyInfo) {
	if(!xc.autocrop) {
	  xc.autocrop = 1;
	  xc.autocrop_autodetect = 1;
	} else if(xc.autocrop_autodetect) {
	  xc.autocrop_autodetect = 0;
	} else {
	  xc.autocrop = 0;
	}
	cXinelibDevice::Instance().ConfigurePostprocessing("autocrop", 
							   xc.autocrop ? true : false,
							   xc.AutocropOptions());
      }

      asprintf(&Message, "%s %s %s", tr("Crop letterbox 4:3 to 16:9"), 
	       OnlyInfo ? ":" : "->",
	       !xc.autocrop ? tr("Off") : xc.autocrop_autodetect ? tr("automatic") : tr("On"));
      break;

    case HOTKEY_DEINTERLACE:    
      {
	/* off, on */
	int off = !strcmp(xc.deinterlace_method, "none");
	if(!OnlyInfo) {
	  off = !off;
	  if(off)
	    strcpy(xc.deinterlace_method, "none");
	  else
	    strcpy(xc.deinterlace_method, "tvtime");
	  cXinelibDevice::Instance().ConfigurePostprocessing(xc.deinterlace_method, xc.audio_delay, 
							     compression, xc.audio_equalizer, 
							     xc.audio_surround, xc.speaker_type);
	}
	asprintf(&Message, "%s %s %s", tr("Deinterlacing"), 
		 OnlyInfo ? ":" : "->", 
		 tr(off ? "Off":"On"));
      }
      break;

    case HOTKEY_UPMIX:    
      /* off, on */
      if(!OnlyInfo) {
	xc.audio_upmix = xc.audio_upmix ? 0 : 1;
	cXinelibDevice::Instance().ConfigurePostprocessing(
		  "upmix", xc.audio_upmix ? true : false, NULL);
      }
      asprintf(&Message, "%s %s %s", 
	       tr("Upmix stereo to 5.1"), 
	       OnlyInfo ? ":" : "->",
	       tr(xc.audio_upmix ? "On" : "Off"));
      break;

    case HOTKEY_DOWNMIX:    
      /* off, on */
      if(!OnlyInfo) {
	xc.audio_surround = xc.audio_surround ? 0 : 1;
	cXinelibDevice::Instance().ConfigurePostprocessing(
	    xc.deinterlace_method, xc.audio_delay, xc.audio_compression, 
	    xc.audio_equalizer, xc.audio_surround, xc.speaker_type);
      }
      asprintf(&Message, "%s %s %s", 
	       tr("Downmix AC3 to surround"), 
	       OnlyInfo ? ":" : "->",
	       tr(xc.audio_surround ? "On":"Off"));
      break;

      case HOTKEY_PLAYLIST:
	/* Start replaying playlist or file pointed by 
	   symlink $(CONFDIR)/plugins/xineliboutput/default_playlist */
	{
	  struct stat st;
	  char *file;
	  asprintf(&file, "%s%s", cPlugin::ConfigDirectory("xineliboutput"), "/default_playlist");
	  if (lstat(file, &st) == 0) {
	    if (S_ISLNK(st.st_mode)) {
	      char *buffer = ReadLink(file);
	      if (!buffer || stat(buffer, &st)) {
		asprintf(&Message, tr("Default playlist not found"));
	      } else {
		LOGDBG("Replaying default playlist: %s", file);
		cControl::Shutdown();
		cControl::Launch(new cXinelibPlayerControl(CloseOsd, buffer));
	      }
	      free(buffer);
	    } else {
	      asprintf(&Message, tr("Default playlist is not symlink"));
	    }
	  } else {
	    asprintf(&Message, tr("Default playlist not defined"));
	  }
	  free(file);
	}
	break;

    default:
      asprintf(&Message, tr("xineliboutput: hotkey %s not binded"), cKey::ToString(Key));
      break;
  }

  if(Message) {
    if(!g_PendingMenuAction &&
       !cRemote::HasKeys() &&
       cRemote::CallPlugin("xineliboutput"))
      g_PendingMenuAction = new cDisplayMessage(Message);
    free(Message);
  }

  g_LastHotkeyTime = now;
  g_LastHotkey = Key;

  return NewState;
}
