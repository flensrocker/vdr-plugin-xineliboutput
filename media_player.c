/*
 * media_player.c: 
 *
 * See the main source file '.c' for copyright information and
 * how to reach the author.
 *
 * $Id: media_player.c,v 1.2 2006-06-04 05:45:37 phintuka Exp $
 *
 */

#include <unistd.h>

#include <vdr/config.h>
#include <vdr/status.h>
#include <vdr/interface.h>

#include "config.h"
#include "media_player.h"
#include "device.h"


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


//
// cXinelibPlayer
//

class cXinelibPlayer : public cPlayer {
  private:
    char *m_File;
    char *m_ResumeFile;

  protected:
    virtual void Activate(bool On);

  public:
    cXinelibPlayer(const char *file);
    virtual ~cXinelibPlayer();
};

cXinelibPlayer::cXinelibPlayer(const char *file) 
{
  m_File = strdup(file);
  m_ResumeFile = NULL;
  asprintf(&m_ResumeFile, "%s.resume", m_File);
}

cXinelibPlayer::~cXinelibPlayer()
{
  Activate(false);
  Detach();
  
  free(m_File);
  m_File = NULL;
  free(m_ResumeFile);
  m_ResumeFile = NULL;
}

void cXinelibPlayer::Activate(bool On)
{
  int pos = 0, fd=-1;
  if(On) {
    if(0 <= (fd = open(m_ResumeFile,O_RDONLY))) {
      if(read(fd, &pos, sizeof(int)) != sizeof(int))
	 pos = 0;
      close(fd);
    }
    cXinelibDevice::Instance().PlayFile(m_File, pos);
  } else {
    pos = cXinelibDevice::Instance().PlayFileCtrl("GETPOS");
    if(pos>=0 && strcasecmp(m_File+strlen(m_File)-4,".ram")) {
      if(0 <= (fd = open(m_ResumeFile, O_WRONLY | O_CREAT, 
			 S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH))) {
	if(write(fd, &pos, sizeof(int)) != sizeof(int))
	  Skins.QueueMessage(mtInfo, "Error writing resume position !", 5, 30);
	close(fd);
      } else {
	Skins.QueueMessage(mtInfo, "Error creating resume file !", 5, 30);
      }
    }
    cXinelibDevice::Instance().PlayFile(NULL,0);
  }
}

//
// cXinelibPlayerControl
//

#include <vdr/skins.h>

cXinelibPlayer *cXinelibPlayerControl::m_Player = NULL;
cMutex cXinelibPlayerControl::m_Lock;
int cXinelibPlayerControl::m_SubtitlePos = 0;

cXinelibPlayerControl::cXinelibPlayerControl(const char *File) :
  cControl(OpenPlayer(File))
{
  char *pt;
  m_DisplayReplay = NULL;
  m_ShowModeOnly = true;
  m_Speed = 1;
  m_File = strdup(File);
  if(NULL != (pt=strrchr(m_File,'/')))
    strcpy(m_File, pt+1);
  if(NULL != (pt=strrchr(m_File,'.')))
    *pt = 0;

#if VDRVERSNUM < 10338
  cStatus::MsgReplaying(this, m_File);
#else
  cStatus::MsgReplaying(this, m_File, File, true);
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
  free(m_File);
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
    Total = cXinelibDevice::Instance().PlayFileCtrl("GETLENGTH");
    m_DisplayReplay->SetTitle(m_File);
    m_DisplayReplay->SetProgress(Current, Total);
    sprintf(t, "%d:%02d:%02d", Total/3600, (Total%3600)/60, Total%60);
    m_DisplayReplay->SetTotal( t );
    sprintf(t, "%d:%02d:%02d", Current/3600, (Current%3600)/60, Current%60);
    m_DisplayReplay->SetCurrent( t );
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
    Hide();
    return osEnd;
  }

  if (m_DisplayReplay) 
    Show();

  int r;
  char *tmp = NULL;
  switch(Key) {
    case kBack:   xc.main_menu_mode = ShowFiles;
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
    //case k1:      r = cXinelibDevice::Instance().PlayFileCtrl("SEEK -600"); break;
    //case k4:      r = cXinelibDevice::Instance().PlayFileCtrl("SEEK +600"); break;
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
    /*case kUp:*/
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
    case kLeft:    Seek(-5);
                   break;
    case kRight:   Seek(5);
                   break;
    case kUp:      Seek(1);  
                   break;
    case kDown:    Seek(-1);
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
