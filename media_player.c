/*
 * media_player.c: 
 *
 * See the main source file 'xineliboutput.c' for copyright information and
 * how to reach the author.
 *
 * $Id: media_player.c,v 1.25 2007-04-09 16:22:40 phintuka Exp $
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
#include "tools/playlist.h"
#include "menu.h"

#include "logdefs.h"

#include "tools/iconv.h"

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

class cXinelibPlayer : public cPlayer 
{
  private:
    cString m_File;
    cString m_ResumeFile;

    cPlaylist m_Playlist;

    bool m_Replaying;
    int  m_Speed; 

  protected:
    virtual void Activate(bool On);

  public:
    cXinelibPlayer(const char *file, bool Queue=false);
    virtual ~cXinelibPlayer();

    // cPlayer
    virtual void SetAudioTrack(eTrackType Type, const tTrackId *TrackId);
    virtual bool GetIndex(int &Current, int &Total, bool SnapToIFrame = false);
    virtual bool GetReplayMode(bool &Play, bool &Forward, int &Speed);

    // cXinelibPlayer
    void Control(const char *s) { (void)cXinelibDevice::Instance().PlayFileCtrl(s); }
    void Control(const char *s, int i) { 
      cString cmd = cString::sprintf(s, i);
      Control(cmd); 
    }
    void SetSpeed(int Speed);
    int  Speed(void) { return m_Speed; };

    bool NextFile(int step);
    bool Replaying(void)  { return m_Replaying; }

    bool m_UseResume;

    /* Playlist access */
    cPlaylist& Playlist(void) { return m_Playlist; }
    const cString& File(void) { return m_File; }
    int  CurrentFile(void) { return m_Playlist.Current()->Index(); } 
    int  Files(void) { return m_Playlist.Count(); }
};

cXinelibPlayer::cXinelibPlayer(const char *file, bool Queue) 
{
  m_ResumeFile = NULL;
  m_UseResume = true;
  m_Replaying = false;
  m_Speed = 1;

  if(file) {
    int len = strlen(file);
    if(len && file[len-1] == '/') { 
      // whole directory, create temporary playlist
      m_Playlist.Read(file, true);
      m_Playlist.Sort();
    } else if(xc.IsPlaylistFile(file)) {
      m_Playlist.Read(file);
    } else if(xc.IsAudioFile(file) && !Queue) {
      // one audio file, create temporary playlist
      cString folder(file);
      *(strrchr(*folder, '/') + 1) = 0;
      m_Playlist.Read(*folder);
      m_Playlist.Sort();
      // search start position
      m_Playlist.SetCurrent(NULL);
      for(cPlaylistItem *i = m_Playlist.First(); i; i = m_Playlist.Next(i))
	if(!strcmp(file, *(i->Filename)))
	  m_Playlist.SetCurrent(i);
    } else {
      // not audio or playlist file, create playlist with only one item
      m_Playlist.Read(file);
    }

    if(m_Playlist.Count() < 1)
      LOGMSG("cXinelibPlayer: nothing to play !");

    if(m_Playlist.Count() > 1)
      m_Playlist.StartScanner();

    m_File = m_Playlist.Current()->Filename;
  }
}

cXinelibPlayer::~cXinelibPlayer()
{
  Activate(false);
  Detach();
}

void cXinelibPlayer::SetAudioTrack(eTrackType Type, const tTrackId *TrackId)
{
  /*LOGMSG("cXinelibPlayer::SetAudioTrack(%d)",(int)Type);*/
  if(IS_DOLBY_TRACK(Type))
    Control("AUDIOSTREAM AC3 %d", (int)(Type - ttDolbyFirst));
  if(IS_AUDIO_TRACK(Type))
    Control("AUDIOSTREAM AC3 %d", (int)(Type - ttAudioFirst));
}

bool cXinelibPlayer::GetIndex(int &Current, int &Total, bool SnapToIFrame) 
{ 
  // Returns the current and total frame index, optionally snapped to the
  // nearest I-frame.
  int msCurrent = cXinelibDevice::Instance().PlayFileCtrl("GETPOS");
  int msTotal   = cXinelibDevice::Instance().PlayFileCtrl("GETLENGTH");
  if(msCurrent>=0 && msTotal>=0) {
    Current = msCurrent * 25 / 1000;
    Total = msTotal * 25 / 1000;
    return true;
  }
  return false; 
}

bool cXinelibPlayer::GetReplayMode(bool &Play, bool &Forward, int &Speed) 
{
  // Returns the current replay mode (if applicable).
  // 'Play' tells whether we are playing or pausing, 'Forward' tells whether
  // we are going forward or backward and 'Speed' is -1 if this is normal
  // play/pause mode, 0 if it is single speed fast/slow forward/back mode
  // and >0 if this is multi speed mode.
  Play = (m_Speed>0);
  Forward = true;
  Speed = abs(m_Speed) - 2;
  if(Speed<-1) Speed=-1;

  return true; 
}

void cXinelibPlayer::SetSpeed(int Speed)
{
  m_Speed = Speed;
  switch(Speed) {
    case -4: Control("TRICKSPEED 8");   break;
    case -3: Control("TRICKSPEED 4");   break;
    case -2: Control("TRICKSPEED 2");   break;
    case  0: Control("TRICKSPEED 0");   break;
    default: m_Speed = 1;
    case  1: Control("TRICKSPEED 1");   break;
    case  2: Control("TRICKSPEED -2");  break;
    case  3: Control("TRICKSPEED -4");  break;
    case  4: Control("TRICKSPEED -12"); break;
  }
}

bool cXinelibPlayer::NextFile(int step)
{
  if(m_Playlist.Count()>1) {
    for(;step < 0; step++)
      m_Playlist.Prev();
    for(;step > 0; step--) 
      m_Playlist.Next();

    if(!m_Playlist.Current())
      LOGERR("!m_Playlist.Get(m_CurrInd)");
    m_File = *m_Playlist.Current()->Filename;
    m_ResumeFile = NULL;
    
    Activate(true);
    if(!m_Replaying)
      return false;
    return true;
  }
  
  return false;
}

void cXinelibPlayer::Activate(bool On)
{
  int pos = 0, len = 0, fd = -1;
  if(On) {
    if(m_UseResume && !*m_ResumeFile)
      m_ResumeFile = cString::sprintf("%s.resume", *m_File);
    if(m_UseResume && 0 <= (fd = open(m_ResumeFile, O_RDONLY))) {
      if(read(fd, &pos, sizeof(int)) != sizeof(int))
	pos = 0;
      close(fd);
    }
    m_Replaying = cXinelibDevice::Instance().PlayFile(m_File, pos);
    LOGDBG("cXinelibPlayer playing %s (%s)", *m_File, m_Replaying?"OK":"FAIL");

    if(m_Replaying) {
      // update playlist metainfo
      const char *tr = cXinelibDevice::Instance().GetMetaInfo(miTrack);
      const char *al = cXinelibDevice::Instance().GetMetaInfo(miAlbum);
      const char *ar = cXinelibDevice::Instance().GetMetaInfo(miArtist);
      if(tr && tr[0] && (!*m_Playlist.Current()->Track || !strstr(m_Playlist.Current()->Track, tr)))
	m_Playlist.Current()->Track = tr;
      if(al && al[0])
	m_Playlist.Current()->Album = al;
      if(ar && ar[0])
	m_Playlist.Current()->Artist = ar;

      // cdda tracks
      if(m_Playlist.Count() == 1 && !strcmp("cdda:/", m_Playlist.First()->Filename)) {
	int count = cXinelibDevice::Instance().PlayFileCtrl("GETAUTOPLAYSIZE");
	if(count>1) {
	  for(int i=0; i<count; i++)
	    m_Playlist.Read(cString::sprintf("cdda:/%d", i+1));
	  m_Playlist.Del(m_Playlist.First());
	}
      }
    }
  } else {
    if(m_UseResume && *m_ResumeFile) {
      pos = cXinelibDevice::Instance().PlayFileCtrl("GETPOS");
      len = cXinelibDevice::Instance().PlayFileCtrl("GETLENGTH");
      if(pos>10000 && pos < (len-10000)) {
	pos = (pos/1000) - 10; // skip back 10 seconds ("VDR style")
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
      m_ResumeFile = NULL;
    }
    cXinelibDevice::Instance().PlayFile(NULL,0);
    m_Replaying = false;
  }
}

//
// cPlaylistMenu
//


class cPlaylistMenu : public cOsdMenu, cPlaylistChangeNotify
{
  protected:

    cPlaylist& m_Playlist;
    bool       m_NeedsUpdate;
    bool&      m_RandomPlay;
    cIConv     ic;

  public:

    cPlaylistMenu(cPlaylist &Playlist, bool& RandomPlay);
    virtual ~cPlaylistMenu();

    void Set(bool setCurrentPlaying = false);
    void SetCurrentExt(int i);
    void SetHelpButtons(void);

    // cOsdMenu
    virtual eOSState ProcessKey(eKeys Key);

    // cPlaylistChangeNotify
    virtual void PlaylistChanged(const cPlaylistItem *item);
};

cPlaylistMenu::cPlaylistMenu(cPlaylist &Playlist, bool& RandomPlay) : 
     cOsdMenu(tr("Playlist")),
     m_Playlist(Playlist),
     m_RandomPlay(RandomPlay),
     ic()
{
  SetTitle(cString::sprintf("%s: %s", tr("Playlist"), *ic.Translate(Playlist.Name())));
  Playlist.Listen(this);
  Set(true);
}

cPlaylistMenu::~cPlaylistMenu()
{
  m_Playlist.Listen(NULL);
}

void cPlaylistMenu::PlaylistChanged(const cPlaylistItem *item)
{
  m_NeedsUpdate = true; 
}

eOSState cPlaylistMenu::ProcessKey(eKeys Key) 
{
  bool hadSubMenu = HasSubMenu();

  if(m_NeedsUpdate)
    Set();

  eOSState state = cOsdMenu::ProcessKey(Key);

  if(state == osUnknown) {
    switch(Key) {
      case kBack:   
                    return osEnd;
      case kRed:    
                    m_RandomPlay = !m_RandomPlay;
	            SetHelpButtons();
	            return osContinue;
      case kGreen:  
                    return AddSubMenu(cMenuXinelib::CreateMenuBrowseFiles(ShowMusic));
      case kYellow: if(m_Playlist.Count() > 1) {
	              eOSState result = osContinue;
	              cPlaylistItem *i = m_Playlist.Current();
		      if(i->Index() == Current()) {
			if(i->Next())
			  result = (eOSState)(os_User + i->Index()); /* forces jump to next item */
			else
			  result = (eOSState)(os_User + i->Index() - 1);/* forces jump to last item */
		      }
		      for(i = m_Playlist.First(); i && i->Index() != Current(); i = m_Playlist.Next(i));
		      if(i)
			m_Playlist.Del(i);
		      if(Current() == Count()-1)
			SetCurrent(Get(Current()-1));
		      Set();
	              return result;
                    }
      case kBlue:   
                    m_Playlist.Sort();
		    Set();
	            return osContinue;
      default: break;
    }
  }

  if(hadSubMenu && !HasSubMenu())
     Set();

  return state;
}

void cPlaylistMenu::SetCurrentExt(int i) 
{
  SetCurrent(Get(i));
  Set();
}

void cPlaylistMenu::SetHelpButtons(void)
{
  SetHelp(!m_RandomPlay ? tr("Button$Random") : tr("Button$Normal"),
	  tr("Button$Add files"),
	  m_Playlist.Count()>1 ? tr("Button$Remove") : NULL,
	  tr("Button$Sort"));
  Display();
}

void cPlaylistMenu::Set(bool setCurrentPlaying)
{
  m_NeedsUpdate = false;

  int currentItem = Current();
  Clear();
  SetHasHotkeys();
  SetCols(2, 30);
  SetHelpButtons();

  int currentPlaying = m_Playlist.Current()->Index();
  int j = 0;

  for(cPlaylistItem *i = m_Playlist.First(); i; i = m_Playlist.Next(i), j++) {
    cString Title;
    if(*i->Artist || *i->Album)
      Title = cString::sprintf("%c\t%s\t(%s%s%s)", 
			       j==currentPlaying ? '*':' ',
			       *i->Track,
			       *i->Artist ?: "",
			       *i->Artist ? ": " : "",
			       *i->Album ?: "");
    else
      Title = cString::sprintf("%c\t%s",
			       j==currentPlaying ? '*':' ',
			       *i->Track);
    Add(new cOsdItem( ic.Translate(Title), (eOSState)(os_User + j)));
  }

  if(setCurrentPlaying)
    SetCurrent(Get(currentPlaying));
  else
    SetCurrent(Get(currentItem));
  
  Display();
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
  m_PlaylistMenu = NULL;
  m_ShowModeOnly = true;
  m_Mode = Mode;
  m_RandomPlay = false;
  m_AutoShowStart = time(NULL);
  m_BlinkState = true;

  m_Player->m_UseResume = (Mode==ShowFiles);

#if VDRVERSNUM < 10338
  cStatus::MsgReplaying(this, *m_Player->File());
#else
  cStatus::MsgReplaying(this, *m_Player->Playlist().Current()->Track, *m_Player->File(), true);
#endif
}

cXinelibPlayerControl::~cXinelibPlayerControl()
{
  if(m_PlaylistMenu) {
    delete m_PlaylistMenu;
    m_PlaylistMenu = NULL;
  }
  if(m_DisplayReplay) {
    delete m_DisplayReplay;
    m_DisplayReplay = NULL;
  }

#if VDRVERSNUM < 10338
  cStatus::MsgReplaying(this, NULL);
#else
  cStatus::MsgReplaying(this, NULL, NULL, false);
#endif

  Close();
}

void cXinelibPlayerControl::Queue(const char *file)
{
  if(!file)
    return;

  m_Lock.Lock();

  LOGMSG("cXinelibPlayerControl::Queue(%s)", file);

  if(!m_Player) {
    OpenPlayer(file, true);
    cControl::Launch(new cXinelibPlayerControl(ShowMusic, NULL));
  } else {
    int len = strlen(file);
    if(len && file[len-1] == '/')
      m_Player->Playlist().Read(file, true);
    else
      m_Player->Playlist().Read(file);
  }

  Skins.Message(mtInfo, tr("Queued to playlist"));

  m_Lock.Unlock();
}

cXinelibPlayer *cXinelibPlayerControl::OpenPlayer(const char *File, bool Queue)
{
  m_Lock.Lock();
  if(!m_Player)
    m_Player = new cXinelibPlayer(File, Queue);
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
  bool Play    = (m_Player->Speed() > 0);
  bool Forward = true;
  int  Speed   = abs(m_Player->Speed()) - 2;
  if(Speed<-1) Speed=-1;

  if(!m_DisplayReplay) {
    if(cOsd::IsOpen())
      return;
    m_DisplayReplay = Skins.Current()->DisplayReplay(m_ShowModeOnly);
  }

  if(!m_ShowModeOnly) {
    char t[128] = "";
    int  Current = cXinelibDevice::Instance().PlayFileCtrl("GETPOS");
    int  Total   = cXinelibDevice::Instance().PlayFileCtrl("GETLENGTH");
    if(Current>=0) m_CurrentPos = Current;
    if(Total>=0) m_CurrentLen = Total;

    if(m_CurrentLen >= 0 /*&& Total >= 0*/) {
      Total = (m_CurrentLen + 500) / 1000;   // ms --> s
      Current = (m_CurrentPos + 500) / 1000;

      cString Title = m_Player->Playlist().Current()->Track;
      if(*m_Player->Playlist().Current()->Artist ||
	 *m_Player->Playlist().Current()->Album)
	Title = cString::sprintf("%s (%s%s%s)", *Title,
				 *m_Player->Playlist().Current()->Artist ?: "",
				 *m_Player->Playlist().Current()->Artist ? ": " : "",
				 *m_Player->Playlist().Current()->Album ?: "");
      else if (cXinelibDevice::Instance().GetMetaInfo(miTitle)[0])
	Title = cString::sprintf("%s (%s)", *Title, 
				 cXinelibDevice::Instance().GetMetaInfo(miTitle));

      cIConv ic;
      m_DisplayReplay->SetTitle(ic.Translate(Title));

      m_DisplayReplay->SetProgress(Current, Total);
      sprintf(t, "%d:%02d:%02d", Total/3600, (Total%3600)/60, Total%60);
      m_DisplayReplay->SetTotal( t );
      sprintf(t, "%d:%02d:%02d", Current/3600, (Current%3600)/60, Current%60);
      m_BlinkState = (m_Player->Speed() != 0) || (!m_BlinkState);
      m_DisplayReplay->SetCurrent( m_BlinkState ? t : "");
    }
  }

  m_DisplayReplay->SetMode(Play, Forward, Speed);

  m_DisplayReplay->Flush();
}

void cXinelibPlayerControl::Hide()
{
  if(m_PlaylistMenu) {
    delete m_PlaylistMenu;
    m_PlaylistMenu = NULL;
  }
  if(m_DisplayReplay) {
    delete m_DisplayReplay;
    m_DisplayReplay = NULL;
  }
}

cOsdObject *cXinelibPlayerControl::GetInfo(void)
{
  /* ??? */
  return NULL;
}

eOSState cXinelibPlayerControl::ProcessKey(eKeys Key)
{
  if (cXinelibDevice::Instance().EndOfStreamReached() ||
      !m_Player->Replaying() ) {
    LOGDBG("cXinelibPlayerControl: EndOfStreamReached");
    LOGDBG("cXinelibPlayerControl: Replaying = %d", m_Player->Replaying());
    int Jump = 1;
    if(m_RandomPlay) {
      srand((unsigned int)time(NULL));
      Jump = (random() % m_Player->Files()) - m_Player->CurrentFile();
    } 
    if(m_Player->Files() < 2 || !m_Player->NextFile(Jump)) {
      Hide();
      return osEnd;
    }
    if(m_PlaylistMenu) {
      m_PlaylistMenu->PlaylistChanged(m_Player->Playlist().Current());
      m_PlaylistMenu->SetCurrentExt(m_Player->CurrentFile());
    }

    if(!m_DisplayReplay)
      m_AutoShowStart = time(NULL);

#if VDRVERSNUM < 10338
    cStatus::MsgReplaying(this, NULL);
    cStatus::MsgReplaying(this, *m_Player->File());
#else
    cStatus::MsgReplaying(this, NULL, NULL, false);
    cStatus::MsgReplaying(this, *m_Player->Playlist().Current()->Track, *m_Player->File(), true);
#endif
  }

  else {
    // metainfo may change during playback (DVD titles, CDDA tracks)
    const char *tr = cXinelibDevice::Instance().GetMetaInfo(miTrack);
    if(tr && tr[0] && (!*m_Player->Playlist().Current()->Track ||
		       !strstr(m_Player->Playlist().Current()->Track, tr))) {
      const char *al = cXinelibDevice::Instance().GetMetaInfo(miAlbum);
      const char *ar = cXinelibDevice::Instance().GetMetaInfo(miArtist);
      LOGDBG("metainfo changed: %s->%s %s->%s %s->%s",
	     *m_Player->Playlist().Current()->Artist?:"-", ar?:"-", 
	     *m_Player->Playlist().Current()->Album ?:"-", al?:"-", 
	     *m_Player->Playlist().Current()->Track ?:"-", tr?:"-");
      m_Player->Playlist().Current()->Track = tr;
      if(al && al[0])
	m_Player->Playlist().Current()->Album = al;
      if(ar && ar[0])
	m_Player->Playlist().Current()->Artist = ar;
#if VDRVERSNUM < 10338
    cStatus::MsgReplaying(this, NULL);
    cStatus::MsgReplaying(this, *m_Player->File());
#else
    cStatus::MsgReplaying(this, NULL, NULL, false);
    cStatus::MsgReplaying(this, *m_Player->Playlist().Current()->Track, *m_Player->File(), true);
#endif
    }
  }

  if(m_PlaylistMenu) {
    m_AutoShowStart = 0;

    eOSState state = osUnknown;

    switch(state=m_PlaylistMenu->ProcessKey(Key)) {
      case osBack:
      case osEnd:   Hide(); break;
      default:      if(state >= os_User) {
	              m_Player->NextFile( (int)state - (int)os_User - m_Player->CurrentFile());
		      m_PlaylistMenu->SetCurrentExt(m_Player->CurrentFile()); 
#if VDRVERSNUM < 10338
		      cStatus::MsgReplaying(this, NULL);
		      cStatus::MsgReplaying(this, *m_Player->File());
#else
		      cStatus::MsgReplaying(this, NULL, NULL, false);
		      cStatus::MsgReplaying(this, *m_Player->Playlist().Current()->Track, 
					    *m_Player->File(), true);
#endif
                    }
	            break;
    }

#if 0
    if(Key != k0 || state != osContinue)
      return osContinue;
#endif
    if(state != osUnknown)
      return osContinue;
  }

  if (m_DisplayReplay) 
    Show();

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
    case kRed:    if(m_Player->Playlist().Count() > 1 || m_Mode == ShowMusic) {
                    Hide();
		    m_PlaylistMenu = new cPlaylistMenu(m_Player->Playlist(), m_RandomPlay);
		    m_AutoShowStart = 0;
                  } else {
		    m_Player->Control("SEEK 0");    break;
		  }
		  break;
    case k0:      if(m_Player->Playlist().Count()>1) {
                    m_RandomPlay = !m_RandomPlay;
		    if(m_RandomPlay)
		      Skins.Message(mtInfo, tr("Random play"));
		    else
		      Skins.Message(mtInfo, tr("Normal play"));
                  }
                  break;
    case kGreen:  m_Player->Control("SEEK -60");  break;
    case kYellow: m_Player->Control("SEEK +60");  break;
    case k1:
    case kUser8:  m_Player->Control("SEEK -20");  break;
    case k3:
    case kUser9:  m_Player->Control("SEEK +20");  break;
    case k2:      m_SubtitlePos -= 10;
    case k5:      m_SubtitlePos += 5;
                  m_Player->Control("SUBTITLES %d", m_SubtitlePos);
                  break;
    case kNext:
    case kRight:  m_Player->NextFile(1);
                  break;
    case kPrev:
    case kLeft:   m_Player->NextFile(-1);
                  break;
    case kDown:
    case kPause:  if(m_Player->Speed()) {
                    m_Player->SetSpeed(0);
		    if(!m_DisplayReplay)
		      m_ShowModeOnly = true;
		    Show();
		    break;
                  }
                  // fall thru
    case kUp:
    case kPlay:   m_Player->SetSpeed(1);
                  if(m_ShowModeOnly && m_DisplayReplay)
		    Hide();
		  else if(m_DisplayReplay)
		    Show();
		  m_ShowModeOnly = false;
                  break;
    case kFastFwd:
#if 1
                  {
                    int speeds[] = { -3, -2, 1, 2, -4, 2, 3, 4, 4 };
		    m_Player->SetSpeed(speeds[m_Player->Speed() + 4]);
		  }
#else
                  switch(m_Player->Speed()) {
                    case  0: m_Player->SetSpeed(-4); break;
                    case -4: m_Player->SetSpeed(-3); break;
		    case -3: m_Player->SetSpeed(-2); break;
		    default:
		    case -2: m_Player->SetSpeed( 1); break;
		    case  1: m_Player->SetSpeed( 2); break;
		    case  2: m_Player->SetSpeed( 3); break;
		    case  3: 
		    case  4: m_Player->SetSpeed( 4); break;
                  }
#endif
                  if(m_Player->Speed() != 1) {
		    Show();
		  } else { 
		    Hide();
		  }
		  break;
    case kFastRew:
#if 1
                  {
                    int speeds[] = { 0, -4, -3, -2, 0, -2, 1, 2, 3 };
		    m_Player->SetSpeed(speeds[m_Player->Speed() + 4]);
		  }
#else
                  switch(m_Player->Speed()) {
                    case  0: 
                    case -4: m_Player->SetSpeed( 0); break;
		    case -3: m_Player->SetSpeed(-4); break;
		    case -2: m_Player->SetSpeed(-3); break;
		    case  1: m_Player->SetSpeed(-2); break;
		    default:
		    case  2: m_Player->SetSpeed( 1); break;
		    case  3: m_Player->SetSpeed( 2); break;
		    case  4: m_Player->SetSpeed( 3); break;
                  }
#endif
                  if(m_Player->Speed() != 1 || !m_ShowModeOnly) {
		    Show();
		  } else {
		    Hide();
		  }
		  break;
    case kOk:     
                  m_AutoShowStart = 0;
                  if(m_Player->Speed() != 1) {
		    Hide();
		    m_ShowModeOnly = !m_ShowModeOnly;
		    Show();
		  } else {
		    if(m_DisplayReplay) {
		      m_ShowModeOnly = true;
		      Hide();
		    } else {
		      Hide();
		      m_ShowModeOnly = false;
		      Show();
		    }
                  }
                  break;
    default:      break;
  }

  if(m_DisplayReplay && 
     m_AutoShowStart &&
     time(NULL) - m_AutoShowStart > 5) {
    m_AutoShowStart = 0;
    Hide();
  }

  if(!m_DisplayReplay && 
     m_AutoShowStart) {
    m_ShowModeOnly = false;
    Show();
  }

  return osContinue;
}

//
// cDvdMenu
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


//
// cXinelibDvdPlayerControl
//

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
    if(Key == kRed)
      Hide();
    else switch(Menu->ProcessKey(Key)) {
      case osUser1: Hide(); m_Player->Control("EVENT XINE_EVENT_INPUT_MENU1"); break;
      case osUser2: Hide(); m_Player->Control("EVENT XINE_EVENT_INPUT_MENU2"); break;
      case osUser3: Hide(); m_Player->Control("EVENT XINE_EVENT_INPUT_MENU3"); break;
      case osUser4: Hide(); m_Player->Control("EVENT XINE_EVENT_INPUT_MENU4"); break;
      case osUser5: Hide(); m_Player->Control("EVENT XINE_EVENT_INPUT_MENU5"); break;
      case osBack:
      case osEnd:   Hide(); break;
      default:      break;
    }
    return osContinue;
  }

  if (m_DisplayReplay) 
    Show();

  bool MenuDomain = false;
  if(Key != kNone) {
    const char *l0 = cXinelibDevice::Instance().GetDvdSpuLang(0);
    const char *l1 = cXinelibDevice::Instance().GetDvdSpuLang(1);
    //const char *t  = cXinelibDevice::Instance().GetMetaInfo(miTitle);
    //const char *dt = cXinelibDevice::Instance().GetMetaInfo(miDvdTitleNo);

    if(/*(dt && !strcmp("0", dt)) ||*/
       (l0 && !strcmp("menu", l0)) ||
       (l1 && !strcmp("menu", l1))) {
      MenuDomain = true;
      //LOGMSG(" *** menu domain %s %s %s %s", l0, l1, t, dt);
    } else {
      //LOGMSG(" *** replay domain %s %s %s %s", l0, l1, t, dt);
    }
  }

  if(MenuDomain) {
    switch(Key) {
      // DVD navigation
      case kUp:    m_Player->Control("EVENT XINE_EVENT_INPUT_UP");     return osContinue;
      case kDown:  m_Player->Control("EVENT XINE_EVENT_INPUT_DOWN");   return osContinue;
      case kLeft:  m_Player->Control("EVENT XINE_EVENT_INPUT_LEFT");   return osContinue;
      case kRight: m_Player->Control("EVENT XINE_EVENT_INPUT_RIGHT");  return osContinue;
      case kOk:    m_Player->Control("EVENT XINE_EVENT_INPUT_SELECT"); return osContinue;
      case kBack:  m_Player->Control("EVENT XINE_EVENT_INPUT_MENU1");  return osContinue;
      default:     break;
    }
  }

  if(!MenuDomain) {
    switch(Key) {
      // Replay control
      case kUp:    Key = kPlay;    break;
      case kDown:  Key = kPause;   break;
      case kLeft:  Key = kFastRew; break;
      case kRight: Key = kFastFwd; break;
      case kOk:
                   if(m_Player->Speed() != 1) {
		     Hide();
		     m_ShowModeOnly = !m_ShowModeOnly;
		     Show();
		   } else {
		     if(m_DisplayReplay) {
		       m_ShowModeOnly = true;
		       Hide();
		     } else {
		       Hide();
		       m_ShowModeOnly = false;
		       Show();
		     }
		   }
		   break;
      case kBack:  xc.main_menu_mode = m_Mode;
	           Hide(); 
		   Close(); 
		   BackToMenu();
		   return osEnd;
      default:     break;
    }
  }

  switch(Key) {
    // DVD menus
    case kRed:    Hide();
                  Menu = new cDvdMenu();
		  break;

    // SPU channel
    case k5:      cXinelibDevice::Instance().SetCurrentDvdSpuTrack(
                       cXinelibDevice::Instance().GetCurrentDvdSpuTrack() - 2);
    case k2:      cRemote::CallPlugin("xineliboutput"); 
                  cRemote::Put(kRed); /* shortcut key */ 
		  cRemote::Put(k2);
                  break;

    // Playback control
    case kGreen:  m_Player->Control("SEEK -60");  break;
    case kYellow: m_Player->Control("SEEK +60");  break;
    case kUser8:
    case k1:      m_Player->Control("SEEK -20");  break;
    case kUser9:
    case k3:      m_Player->Control("SEEK +20");  break;

    case kStop: 
    case kBlue:   Hide();
                  Close();
                  return osEnd;

    case k9:      m_Player->Control("EVENT XINE_EVENT_INPUT_NEXT TITLE"); break;
    case k7:      m_Player->Control("EVENT XINE_EVENT_INPUT_PREVIOUS TITLE"); break;
    case k6:
    case kNext:   m_Player->Control("EVENT XINE_EVENT_INPUT_NEXT CHAPTER"); break;
    case k4:
    case kPrev:   m_Player->Control("EVENT XINE_EVENT_INPUT_PREVIOUS CHAPTER"); break;

    case kFastFwd:
#if 1
                  {
                    int speeds[] = { -3, -2, 1, 2, -4, 2, 3, 4, 4 };
		    m_Player->SetSpeed(speeds[m_Player->Speed() + 4]);
		  }
#else
		  switch(m_Player->Speed()) {
                    case  0: m_Player->SetSpeed(-4); break;
                    case -4: m_Player->SetSpeed(-3); break;
		    case -3: m_Player->SetSpeed(-2); break;
		    default:
		    case -2: m_Player->SetSpeed( 1); break;
		    case  1: m_Player->SetSpeed( 2); break;
		    case  2: m_Player->SetSpeed( 3); break;
		    case  3: 
		    case  4: m_Player->SetSpeed( 4); break;
                  }
#endif
                  if(m_Player->Speed() != 1) {
		    Show();
		  } else { 
		    Hide();
		  }
		  break;
    case kFastRew:
#if 1
                  {
                    int speeds[] = { 0, -4, -3, -2, 0, -2, 1, 2, 3 };
		    m_Player->SetSpeed(speeds[m_Player->Speed() + 4]);
		  }
#else
                  switch(m_Player->Speed()) {
                    case  0: 
                    case -4: m_Player->SetSpeed( 0); break;
		    case -3: m_Player->SetSpeed(-4); break;
		    case -2: m_Player->SetSpeed(-3); break;
		    case  1: m_Player->SetSpeed(-2); break;
		    default:
		    case  2: m_Player->SetSpeed( 1); break;
		    case  3: m_Player->SetSpeed( 2); break;
		    case  4: m_Player->SetSpeed( 3); break;
                  }
#endif
                  if(m_Player->Speed() != 1 || !m_ShowModeOnly) {
		    Show();
		  } else {
		    Hide();
		  }
		  break;
    case kInfo:   if(m_DisplayReplay) {
                    Hide();
                  } else {
                    m_ShowModeOnly = false;
                    Show();
		  }
                  break;
    case kPause:  if(m_Player->Speed()) {
                    m_Player->SetSpeed(0);
		    m_ShowModeOnly = false;
		    Show();
		    break;
                  }
                  // fall thru
    case kPlay:   m_Player->SetSpeed(1);
                  m_ShowModeOnly = true;
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
    cString m_File;
    bool    m_Active;

  protected:
    virtual void Activate(bool On);

  public:
    cXinelibImagePlayer(const char *file); 
    virtual ~cXinelibImagePlayer();

    bool ShowImage(char *file);
};

cXinelibImagePlayer::cXinelibImagePlayer(const char *file) 
{
  m_File = file;
  m_Active = false;
}

cXinelibImagePlayer::~cXinelibImagePlayer()
{
  Activate(false);
  Detach();
}

void cXinelibImagePlayer::Activate(bool On)
{
  if(On) {
    m_Active = true;
    cXinelibDevice::Instance().PlayFile(m_File, 0, true);
  } else {
    m_Active = false;
    cXinelibDevice::Instance().PlayFile(NULL, 0);
  }
}

bool cXinelibImagePlayer::ShowImage(char *file)
{
  m_File = file;
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
  strn0cpy(xc.browse_images_dir, m_Files[m_Index], sizeof(xc.browse_images_dir));
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
