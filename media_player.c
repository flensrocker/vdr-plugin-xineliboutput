/*
 * media_player.c: 
 *
 * See the main source file '.c' for copyright information and
 * how to reach the author.
 *
 * $Id: media_player.c,v 1.8 2006-08-24 09:13:23 phintuka Exp $
 *
 */

#include <unistd.h>

#include <vdr/config.h>
#include <vdr/status.h>
#include <vdr/interface.h>
#include <vdr/tools.h>

#include "config.h"
#include "media_player.h"
#include "device.h"

#include "logdefs.h"

#if VDRVERSNUM < 10400
// Dirty hack to bring menu back ...
#include <vdr/remote.h>
static void BackToMenu(void)
{
  static bool MagicKeyAdded = false;

  if(!MagicKeyAdded) {
    MagicKeyAdded = true;
    cKeyMacro *m = new cKeyMacro();
    char *tmp = strdup("User1\t@xineliboutput");
    m->Parse(tmp);
    free(tmp);
    eKeys *keys = (eKeys*)m->Macro();
    keys[0] = (eKeys)(k_Plugin|0x1000); /* replace kUser1 if it is used to something else */
    keys[1] = k_Plugin;
    
    KeyMacros.Add(m);
  }

  cRemote::PutMacro((eKeys)(k_Plugin|0x1000));
}
#else
static void BackToMenu(void)
{
  cRemote::CallPlugin("xineliboutput");
}
#endif

#define MAX_FILES 256
static char **ScanDir(const char *DirName)
{
  DIR *d = opendir(DirName);
  if (d) {
    LOGDBG("ScanDir(%s)", DirName);
    struct dirent *e;
    int n = 0;
    char **result = (char**)malloc(sizeof(char*)*(MAX_FILES+1));
    char **current = result;
    *current = NULL;
    while ((e = readdir(d)) != NULL) {
      char *buffer = NULL;
      asprintf(&buffer, "%s/%s", DirName, e->d_name);
      struct stat st;
      if (stat(buffer, &st) == 0) {
	if(!S_ISDIR(st.st_mode)) {
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
	  if(xc.IsVideoFile(buffer)) {
	    n++;
	    if(n<MAX_FILES) {
	      *current = buffer;
	      *(++current) = NULL;
	      buffer = NULL;
	      LOGDBG("ScanDir: %s", e->d_name);
	    } else {
	      LOGMSG("ScanDir: Found over %d matching files, list truncated!", n);
	      free(buffer);
	      break;
	    }
	  }
	}
      }
      free(buffer);
    }
    LOGDBG("ScanDir: Found %d matching files", n);
    closedir(d);
    return result;
  }

  LOGERR("ScanDir: Error opening %s", DirName);
  return NULL;
}

static char **Read_m3u(const char *file)
{
  FILE *f = fopen(file, "r");
  if(f) {
    LOGDBG("Read_m3u(%s)", file);
    int n = 0;
    char **result = (char**)malloc(sizeof(char*)*(MAX_FILES+1));
    char **current = result, *pt;
    char *base = strdup(file);
    *current = NULL;
    if(NULL != (pt=strrchr(base,'/')))
      pt[1]=0;
    cReadLine r;
    while(NULL != (pt=r.Read(f)) && n < MAX_FILES) {
      if(*pt == '#' || !*pt)
	continue;
      if(*pt == '/' || 
	 (strstr(pt,"://")+1 == strchr(pt,'/') && 
	  strchr(pt,'/') - pt < 8))
	*current = strdup(pt);
      else
	asprintf(current, "%s/%s", base, pt);
      LOGDBG("Read_m3u: %s", *current);
      *(++current) = NULL;
      n++;
    }
    free(base);
    if(n >= MAX_FILES) 
      LOGMSG("Read_m3u: Found over %d matching files, list truncated!", n);
    LOGDBG("Read_m3u: Found %d matching files", n);
    return result;
  }
  LOGERR("Read_m3u: Error opening %s", file);
  return NULL;
}

//
// cXinelibPlayer
//

class cXinelibPlayer : public cPlayer {
  private:
    char *m_File;
    char *m_ResumeFile;
    char *m_Title;

    char **m_Playlist;
    int  m_CurrInd;

    bool m_Replaying;

  protected:
    virtual void Activate(bool On);

  public:
    cXinelibPlayer(const char *file);
    virtual ~cXinelibPlayer();

    const char *Title(void);    
    const char *File(void);

    bool NextFile(int step);
    bool Replaying(void) { return m_Replaying; }

    bool m_UseResume;
};

cXinelibPlayer::cXinelibPlayer(const char *file) 
{
  int len = strlen(file);

  m_Playlist = NULL;
  m_ResumeFile = NULL;
  m_UseResume = true;
  m_Title = NULL;
  m_CurrInd = 0;
  m_Replaying = false;
  if(len && file[len-1] == '/') {
    m_Playlist = ScanDir(file);
  } else if(len>4 && !strncasecmp(file+len-4, ".m3u", 4)) {
    m_Playlist = Read_m3u(file);
  }
  if(m_Playlist && !m_Playlist[0]) {
    free(m_Playlist);
    m_Playlist = NULL;
  }

  m_File = strdup(m_Playlist ? m_Playlist[m_CurrInd] : file);
}

cXinelibPlayer::~cXinelibPlayer()
{
  Activate(false);
  Detach();

  if(m_Playlist) {
    int i=0;
    while(m_Playlist[i])
      free(m_Playlist[i++]);
    free(m_Playlist);
  }
  free(m_File);
  m_File = NULL;
  free(m_ResumeFile);
  m_ResumeFile = NULL;
  free(m_Title);
  m_Title = NULL;
}

const char *cXinelibPlayer::Title(void)
{
  char *pt;

  if(!m_Title) {
    if(NULL != (pt=strrchr(m_File,'/')))
      m_Title = strdup(pt+1);
    else
      m_Title = strdup(m_File);
    
    if(NULL != (pt=strrchr(m_Title,'.')))
      *pt = 0;
  }

  return m_Title;
}

const char *cXinelibPlayer::File(void)
{
  return m_File;
}

bool cXinelibPlayer::NextFile(int step)
{
  step = step<-1?-1 : (step>1?1 : step);
  if(m_Playlist && ((step==1  && m_Playlist[m_CurrInd+1]) || 
		    (step==-1 && m_CurrInd>0))){
    free(m_File);
    free(m_ResumeFile);
    free(m_Title);
    m_ResumeFile = NULL;
    m_Title = NULL;
    
    m_CurrInd += step;

    m_File = strdup(m_Playlist[m_CurrInd]);

    Activate(true);
    if(!m_Replaying)
      return false;

    return true;

  } else if(m_CurrInd == 0 && step<0) {
    cXinelibDevice::Instance().PlayFileCtrl("SEEK 0");
    return true;
  }
  
  return false;
}

void cXinelibPlayer::Activate(bool On)
{
  int pos = 0, fd = -1;
  if(On) {
    if(m_UseResume && !m_ResumeFile)
      asprintf(&m_ResumeFile, "%s.resume", m_File);
    if(m_UseResume && 0 <= (fd = open(m_ResumeFile,O_RDONLY))) {
      if(read(fd, &pos, sizeof(int)) != sizeof(int))
	pos = 0;
      close(fd);
    }
    m_Replaying = cXinelibDevice::Instance().PlayFile(m_File, pos);
    LOGDBG("cXinelibPlayer playing %s (%s)", m_File, m_Replaying?"OK":"FAIL");
  } else {
    if(m_UseResume && m_ResumeFile) {
      pos = cXinelibDevice::Instance().PlayFileCtrl("GETPOS");
      if(strcasecmp(m_File+strlen(m_File)-4,".ram")) {
	if(pos>=0) {
	  pos /= 1000;
	  if(0 <= (fd = open(m_ResumeFile, O_WRONLY | O_CREAT, 
			     S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH))) {
	    if(write(fd, &pos, sizeof(int)) != sizeof(int)) {
	      Skins.QueueMessage(mtInfo, "Error writing resume position !", 5, 30);
	    }
	    close(fd);
	  } else {
	    Skins.QueueMessage(mtInfo, "Error creating resume file !", 5, 30);
	  }
	} else {
	  unlink(m_ResumeFile);
	}
      }
      free(m_ResumeFile);
      m_ResumeFile = NULL;
    }
    cXinelibDevice::Instance().PlayFile(NULL,0);
    m_Replaying = false;
  }
}

//
// cXinelibPlayerControl
//

#include <vdr/skins.h>

cXinelibPlayer *cXinelibPlayerControl::m_Player = NULL;
cMutex cXinelibPlayerControl::m_Lock;
int cXinelibPlayerControl::m_SubtitlePos = 0;

cXinelibPlayerControl::cXinelibPlayerControl(eMainMenuMode Mode, const char *File) :
  cControl(OpenPlayer(File))
{
  m_DisplayReplay = NULL;
  m_ShowModeOnly = true;
  m_Speed = 1;
  m_Mode = Mode;

  m_Player->m_UseResume = (Mode==ShowFiles);

#if VDRVERSNUM < 10338
  cStatus::MsgReplaying(this, m_Player->File());
#else
  cStatus::MsgReplaying(this, m_Player->Title(), m_Player->File(), true);
#endif
}

cXinelibPlayerControl::~cXinelibPlayerControl()
{
  if(m_DisplayReplay)
    delete m_DisplayReplay;
  m_DisplayReplay = NULL;

#if VDRVERSNUM < 10338
  cStatus::MsgReplaying(this, NULL);
#else
  cStatus::MsgReplaying(this, NULL, NULL, false);
#endif
  Close();
}

cXinelibPlayer *cXinelibPlayerControl::OpenPlayer(const char *File)
{
  m_Lock.Lock();
  if(!m_Player)
    m_Player = new cXinelibPlayer(File);
  m_Lock.Unlock();
  return m_Player;
}

void cXinelibPlayerControl::Close(void)
{
  m_Lock.Lock();
  if(m_Player)
    delete m_Player;
  m_Player = NULL;
  m_Lock.Unlock();
}

void cXinelibPlayerControl::Show()
{
  bool Play = (m_Speed!=0), Forward = true;
  int  Speed = -1;

  if(!m_DisplayReplay) {
    m_DisplayReplay = Skins.Current()->DisplayReplay(m_ShowModeOnly);
  }

  if(!m_ShowModeOnly) {
    char t[128] = "";
    int Current, Total;
    Current = cXinelibDevice::Instance().PlayFileCtrl("GETPOS");
    Total   = cXinelibDevice::Instance().PlayFileCtrl("GETLENGTH");
    if(Current>=0 && Total>=0) {
      Total = (Total+500)/1000;
      Current = (Current+500)/1000;
      m_DisplayReplay->SetTitle(m_Player->Title());
      m_DisplayReplay->SetProgress(Current, Total);
      sprintf(t, "%d:%02d:%02d", Total/3600, (Total%3600)/60, Total%60);
      m_DisplayReplay->SetTotal( t );
      sprintf(t, "%d:%02d:%02d", Current/3600, (Current%3600)/60, Current%60);
      m_DisplayReplay->SetCurrent( t );
    }
  }

  m_DisplayReplay->SetMode(Play, Forward, Speed);
  m_DisplayReplay->Flush();
}

void cXinelibPlayerControl::Hide()
{
  if(m_DisplayReplay) {
    delete m_DisplayReplay;
    m_DisplayReplay = NULL;
  }
}

eOSState cXinelibPlayerControl::ProcessKey(eKeys Key)
{
  if (cXinelibDevice::Instance().EndOfStreamReached()) {
    LOGDBG("cXinelibPlayerControl: EndOfStreamReached");
    if(!m_Player->NextFile(1)) {
      Hide();
      return osEnd;
    }
  }

  if (m_DisplayReplay) 
    Show();

  int r;
  char *tmp = NULL;
  switch(Key) {
    case kBack:   xc.main_menu_mode = m_Mode;
                  Hide(); 
                  Close(); 
		  BackToMenu();
                  return osEnd;
    case kStop:
    case kBlue:   Hide();
                  Close();
                  return osEnd;
    case kRed:    r = cXinelibDevice::Instance().PlayFileCtrl("SEEK 0");    break;
    case kGreen:  r = cXinelibDevice::Instance().PlayFileCtrl("SEEK -60");  break;
    case kYellow: r = cXinelibDevice::Instance().PlayFileCtrl("SEEK +60");  break;
    case k1:
    case kUser8:  r = cXinelibDevice::Instance().PlayFileCtrl("SEEK -20");  break;
    case k3:
    case kUser9:  r = cXinelibDevice::Instance().PlayFileCtrl("SEEK +20");  break;
    case k2:      m_SubtitlePos -= 10;
    case k5:      m_SubtitlePos += 5;
                  asprintf(&tmp,"SUBTITLES %d",m_SubtitlePos);
                  r = cXinelibDevice::Instance().PlayFileCtrl(tmp);
                  free(tmp);
                  break;
    case kNext:
    case kRight:  m_Player->NextFile(1);
                  break;
    case kPrev:
    case kLeft:   m_Player->NextFile(-1);
                  break;
    case kDown:
    case kPause:  if(m_Speed != 0) {
                    r = cXinelibDevice::Instance().PlayFileCtrl("TRICKSPEED 0");
		    if(!m_DisplayReplay)
		      m_ShowModeOnly = true;
		    m_Speed = 0;
		    Show();
		    break;
                  }
                  // fall thru
    case kPlay:   r = cXinelibDevice::Instance().PlayFileCtrl("TRICKSPEED 1"); 
		  m_Speed = 1;
                  if(m_ShowModeOnly && m_DisplayReplay)
		    Hide();
		  else if(m_DisplayReplay)
		    Show();
		  m_ShowModeOnly = false;
                  break;
    case kOk:     if(m_DisplayReplay) {
                    if(m_Speed==0) {
		      Hide();
                      m_ShowModeOnly = !m_ShowModeOnly;
		      Show();
                    } else if(m_ShowModeOnly) {
                      Hide();
		    } else {
		      Hide();
		      m_ShowModeOnly = true;
		      Show();
		    }
                  } else {
                    m_ShowModeOnly = false;
                    Show();
		  }
                  break;
    default:      break;
  }

  return osContinue;
}

//
// cXinelibDvdPlayerControl
//

class cDvdMenu : public cOsdMenu {
 public:
  cDvdMenu(void) : cOsdMenu("DVD Menu")
  {
    Add(new cOsdItem("Exit DVD menu",  osUser1));
    Add(new cOsdItem("DVD Root menu",  osUser2));
    Add(new cOsdItem("DVD Title menu", osUser3));
    Add(new cOsdItem("DVD SPU menu",   osUser4));
    Add(new cOsdItem("DVD Audio menu", osUser5));
    Add(new cOsdItem("Close menu",     osEnd));
    Display();
  }
};

cXinelibDvdPlayerControl::~cXinelibDvdPlayerControl() 
{
  if(Menu) {
    delete Menu;
    Menu = NULL;
  }
}

void cXinelibDvdPlayerControl::Hide(void)
{
  if(Menu) {
    delete Menu;
    Menu = NULL;
  }
  cXinelibPlayerControl::Hide();
}

void cXinelibDvdPlayerControl::Show(void)
{
  if(!Menu)
    cXinelibPlayerControl::Show();
  else
    cXinelibPlayerControl::Hide();
}

eOSState cXinelibDvdPlayerControl::ProcessKey(eKeys Key)
{
  if (cXinelibDevice::Instance().EndOfStreamReached()) {
    Hide();
    return osEnd;
  }

  if(Menu) {
    switch(Menu->ProcessKey(Key)) {
      case osUser1: Hide(); cXinelibDevice::Instance().PlayFileCtrl("EVENT XINE_EVENT_INPUT_MENU1"); break;
      case osUser2: Hide(); cXinelibDevice::Instance().PlayFileCtrl("EVENT XINE_EVENT_INPUT_MENU2"); break;
      case osUser3: Hide(); cXinelibDevice::Instance().PlayFileCtrl("EVENT XINE_EVENT_INPUT_MENU3"); break;
      case osUser4: Hide(); cXinelibDevice::Instance().PlayFileCtrl("EVENT XINE_EVENT_INPUT_MENU4"); break;
      case osUser5: Hide(); cXinelibDevice::Instance().PlayFileCtrl("EVENT XINE_EVENT_INPUT_MENU5"); break;
      case osBack:
      case osEnd:   Hide(); break;
      default:      break;
    }
    return osContinue;
  }

  if (m_DisplayReplay) 
    Show();

  int r;
  switch(Key) {
    // DVD navigation
    case kUp:     r = cXinelibDevice::Instance().PlayFileCtrl("EVENT XINE_EVENT_INPUT_UP"); break;
    case kDown:   r = cXinelibDevice::Instance().PlayFileCtrl("EVENT XINE_EVENT_INPUT_DOWN"); break;
    case kLeft:   r = cXinelibDevice::Instance().PlayFileCtrl("EVENT XINE_EVENT_INPUT_LEFT"); break;
    case kRight:  r = cXinelibDevice::Instance().PlayFileCtrl("EVENT XINE_EVENT_INPUT_RIGHT"); break;
    case kOk:     r = cXinelibDevice::Instance().PlayFileCtrl("EVENT XINE_EVENT_INPUT_SELECT"); break;
    case kBack:   r = cXinelibDevice::Instance().PlayFileCtrl("EVENT XINE_EVENT_INPUT_MENU1"); break;
    case kRed:    Hide();
                  Menu = new cDvdMenu();
		  break;
    // Playback control
    case kGreen:  r = cXinelibDevice::Instance().PlayFileCtrl("SEEK -60");  break;
    case kYellow: r = cXinelibDevice::Instance().PlayFileCtrl("SEEK +60");  break;
    case kStop: 
    case kBlue:   Hide();
                  Close();
                  return osEnd;
    case kNext:   cXinelibDevice::Instance().PlayFileCtrl("EVENT XINE_EVENT_INPUT_NEXT"); break;
    case kPrev:   cXinelibDevice::Instance().PlayFileCtrl("EVENT XINE_EVENT_INPUT_PREVIOUS"); break;
    case kInfo:   if(m_DisplayReplay) {
                    Hide();
                  } else {
                    m_ShowModeOnly = false;
                    Show();
		  }
                  break;
    case kPause:  if(m_Speed != 0) {
                    r = cXinelibDevice::Instance().PlayFileCtrl("TRICKSPEED 0");
		    m_ShowModeOnly = false;
		    m_Speed = 0;
		    Show();
		    break;
                  }
                  // fall thru
    case kPlay:   r = cXinelibDevice::Instance().PlayFileCtrl("TRICKSPEED 1"); 
		  m_Speed = 1;
		  Hide();
                  break;
    default:      break;
  }

  return osContinue;
}

//
// cXinelibImagePlayer
//

class cXinelibImagePlayer : public cPlayer {
  private:
    char *m_File;
    bool m_Active;

  protected:
    virtual void Activate(bool On);

  public:
    cXinelibImagePlayer(const char *file); 
    virtual ~cXinelibImagePlayer();

    bool ShowImage(char *file);
};

cXinelibImagePlayer::cXinelibImagePlayer(const char *file) 
{
  m_File = strdup(file);
  m_Active = false;
}

cXinelibImagePlayer::~cXinelibImagePlayer()
{
  Activate(false);
  Detach();
  
  free(m_File);
  m_File = NULL;
}

void cXinelibImagePlayer::Activate(bool On)
{
  if(On) {
    m_Active = true;
    cXinelibDevice::Instance().PlayFile(m_File, 0, true);
  } else {
    m_Active = false;
    cXinelibDevice::Instance().PlayFile(NULL,0);
  }
}

bool cXinelibImagePlayer::ShowImage(char *file)
{
  free(m_File);
  m_File = strdup(file);
  if(m_Active)
    return cXinelibDevice::Instance().PlayFile(m_File, 0, true);
  return true;
}


//
// cXinelibImagesControl
//

cXinelibImagePlayer *cXinelibImagesControl::m_Player = NULL;
cMutex cXinelibImagesControl::m_Lock;

cXinelibImagesControl::cXinelibImagesControl(char **Files, int Index, int Count) :
  cControl(OpenPlayer(Files[Index]))
{
  m_DisplayReplay = NULL;
  m_Files = Files;
  m_File = NULL;
  m_Index = Index;
  m_Count = Count;
  m_Speed = 0;
  m_ShowModeOnly = false;

  Seek(0);
}

cXinelibImagesControl::~cXinelibImagesControl()
{
  if(m_DisplayReplay)
    delete m_DisplayReplay;
  m_DisplayReplay = NULL;

#if VDRVERSNUM < 10338
  cStatus::MsgReplaying(this, NULL);
#else
  cStatus::MsgReplaying(this, NULL, NULL, false);
#endif
  Close();

  if(m_Files) {
    int i=0;
    while(m_Files[i]) {
      free(m_Files[i]);
      m_Files[i] = NULL;
      i++;
    }
    delete [] m_Files;
    m_Files = NULL;
  }
}

cXinelibImagePlayer *cXinelibImagesControl::OpenPlayer(const char *File)
{
  m_Lock.Lock();
  if(!m_Player)
    m_Player = new cXinelibImagePlayer(File);
  m_Lock.Unlock();
  return m_Player;
}

void cXinelibImagesControl::Close(void)
{
  m_Lock.Lock();
  if(m_Player)
    delete m_Player;
  m_Player = NULL;
  m_Lock.Unlock();
}

void cXinelibImagesControl::Delete(void)
{
  if(Interface->Confirm(tr("Delete image ?"))) {
    if(!unlink(m_Files[m_Index])) {
      free(m_Files[m_Index]);
      for(int i=m_Index; i<m_Count; i++)
	m_Files[i] = m_Files[i+1];
      m_Count--;
      m_Files[m_Count] = NULL;      
      Seek(0);
    }
  }
}

void cXinelibImagesControl::Seek(int Rel)
{
  if(m_Index == m_Count-1 && Rel>0)
    m_Index = 0;
  else if(m_Index == 0 && Rel<0)
    m_Index = m_Count-1;
  else
    m_Index += Rel;

  if(m_Index < 0) 
    m_Index = 0; 
  else if(m_Index >= m_Count) 
    m_Index = m_Count;
  
  char *pt;
  free(m_File);
  m_File = strdup(m_Files[m_Index]);
  if(NULL != (pt=strrchr(m_File,'/')))
    strcpy(m_File, pt+1); 
  if(NULL != (pt=strrchr(m_File,'.')))
    *pt = 0;

#if VDRVERSNUM < 10338
  cStatus::MsgReplaying(this, m_Files[m_Index]);
#else
  cStatus::MsgReplaying(this, m_File, m_Files[m_Index], true);
#endif

  m_Player->ShowImage(m_Files[m_Index]);
  m_LastShowTime = time(NULL);
  strcpy(xc.browse_images_dir, m_Files[m_Index]);
}

void cXinelibImagesControl::Show(void)
{
  bool Play = (m_Speed!=0), Forward = m_Speed>=0;
  int Speed = abs(m_Speed);
 
  if(!m_DisplayReplay) {
    m_DisplayReplay = Skins.Current()->DisplayReplay(m_ShowModeOnly);
  }

  if(!m_ShowModeOnly) {
    char t[128] = "";
    m_DisplayReplay->SetTitle(m_File);
    m_DisplayReplay->SetProgress(m_Index, m_Count);
    sprintf(t, "%d", m_Count);
    m_DisplayReplay->SetTotal( t );
    sprintf(t, "%d", m_Index+1);
    m_DisplayReplay->SetCurrent( t );
  }

  m_DisplayReplay->SetMode(Play, Forward, Speed);
  m_DisplayReplay->Flush();
}

void cXinelibImagesControl::Hide(void)
{
  if(m_DisplayReplay) {
    delete m_DisplayReplay;
    m_DisplayReplay = NULL;
  }
}

eOSState cXinelibImagesControl::ProcessKey(eKeys Key)
{
  switch(Key) {
    case kBack:    xc.main_menu_mode = ShowImages;
                   Hide(); 
                   Close(); 
                   BackToMenu();
                   //return osPlugin;		   
		   return osEnd;
    case kYellow:  Delete(); 
                   break;
    case kStop:
    case kBlue:    Hide();
                   Close();
                   return osEnd;
    case kPrev:
    case kLeft:    Seek(-1);
                   break;
    case kNext:
    case kRight:   Seek(1);
                   break;
    case kUp:      Seek(5);  
                   break;
    case kDown:    Seek(-5);
                   break;
    case kPause:   m_Speed = 0;
                   break;
    case kPlay:    m_Speed = 2;
                   break;
    case kFastFwd: m_Speed++;
                   break;
    case kFastRew: m_Speed--;
                   break;
    case kOk:      if(m_DisplayReplay) {
                     if(m_ShowModeOnly) {
                       Hide();
                       m_ShowModeOnly = false;
                       Show();
                     } else {
		       Hide();
		     }
                   } else {
                     m_ShowModeOnly = true;
                     Show();
		   }
                   break;
    default:       break;
  }

  static const int Speed2Time[] = {0,5,3,1};
  if(m_Speed > 3)
     m_Speed = 3;
  if(m_Speed < -3)
    m_Speed = -3;

  if(Key == kNone && m_Speed != 0) {
    if(m_LastShowTime + Speed2Time[m_Speed<0 ? -m_Speed : m_Speed] <= time(NULL))
      Seek(sgn(m_Speed));
  }

  if (m_DisplayReplay) 
    Show();

  return osContinue;
}
