/*
 * device.c: xine-lib output device for the Video Disk Recorder
 *
 * See the main source file 'xineliboutput.c' for copyright information and
 * how to reach the author.
 *
 * $Id: device.c,v 1.28 2006-12-17 22:10:51 phintuka Exp $
 *
 */

#define __STDC_FORMAT_MACROS
#include <inttypes.h>

#include <vdr/config.h>
#include <vdr/thread.h>
#include <vdr/dvbspu.h>
#include <vdr/channels.h>
#include <vdr/skins.h>
#include <vdr/status.h>
#include <vdr/remote.h>

//#define XINELIBOUTPUT_DEBUG
//#define XINELIBOUTPUT_DEBUG_STDOUT
//#define XINELIBOUTPUT_DEBUG_STDERR
//#define TRACK_EXEC_TIME
#define HD_MODE_TEST
//#define TEST_TRICKSPEEDS
//#define SKIP_DVDSPU
//#define FORWARD_DVD_SPUS
#define DEBUG_SWITCHING_TIME

#include "logdefs.h"
#include "config.h"
#include "osd.h"

#include "tools/listiter.h"
#include "tools/pes.h"
#include "tools/functor.h"

#include "frontend_local.h"
#include "frontend_svr.h"

#include "device.h"

#define STILLPICTURE_REPEAT_COUNT 3

//---------------------------- status monitor -------------------------------

class cXinelibStatusMonitor : public cStatus 
{
  private:
    cXinelibStatusMonitor(); 
    cXinelibStatusMonitor(cXinelibStatusMonitor&); 

  public:
    cXinelibStatusMonitor(cXinelibDevice& device, int cardIndex) :
        m_Device(device), m_cardIndex(cardIndex) 
    {
#ifdef DEBUG_SWITCHING_TIME
      switchtimeOff   = 0LL;
      switchtimeOn    = 0LL;
      switchingIframe = false;
#endif
    };

  protected:
    virtual void ChannelSwitch(const cDevice *Device, int ChannelNumber);
#if VDRVERSNUM < 10338
    virtual void Replaying(const cControl *Control, const char *Name);
#else
    virtual void Replaying(const cControl *Control, const char *Name, 
			   const char *FileName, bool On);
#endif

    cXinelibDevice& m_Device;
    int m_cardIndex;

#ifdef DEBUG_SWITCHING_TIME
  public:
    int64_t switchtimeOff;
    int64_t switchtimeOn;
    bool    switchingIframe;

    void IFrame(void)  
    {
      if(!switchingIframe) {
	int64_t now = cTimeMs::Now();
	switchingIframe = true;
	LOGMSG("Channel switch: off -> on %" PRId64 " ms, "
	       "on -> 1. I-frame %" PRId64 " ms",
	       switchtimeOn-switchtimeOff, now-switchtimeOn);
      } else {
	int64_t now = cTimeMs::Now();
	LOGMSG("Channel switch: on -> 2. I-frame %" PRId64 " ms, "
	       "Total %" PRId64 " ms",
	       now-switchtimeOn, now-switchtimeOff);
	switchtimeOff = 0LL;
	switchtimeOn = 0LL;
	switchingIframe = false;
      }
    }
#endif
};

void cXinelibStatusMonitor::ChannelSwitch(const cDevice *Device, 
					  int ChannelNumber) 
{
  TRACEF("cXinelibStatusMonitor::ChannelSwitch");
  TRACK_TIME(200);

  if (ChannelNumber) {
    if (Device->CardIndex() == m_cardIndex) {
#ifdef DEBUG_SWITCHING_TIME
      switchtimeOn = cTimeMs::Now();
#endif
      m_Device.SetTvMode(Channels.GetByNumber(ChannelNumber));
      TRACE("cXinelibStatusMonitor: Set to TvMode");
    }
  } else {
    if (Device->CardIndex() == m_cardIndex) {
#ifdef DEBUG_SWITCHING_TIME
      switchtimeOff = cTimeMs::Now();
#endif
      m_Device.StopOutput();
      TRACE("cXinelibStatusMonitor: received stop");
    }
  }
}

#if VDRVERSNUM < 10338
void cXinelibStatusMonitor::Replaying(const cControl *Control, 
				      const char *Name) 
{
  TRACEF("cXinelibStatusMonitor::Replaying");
  
  if (Name != NULL) {
    TRACE("cXinelibStatusMonitor: Replaying " << Name);
    m_Device.SetReplayMode();
  }
}
#else
void cXinelibStatusMonitor::Replaying(const cControl *Control, 
				      const char *Name,
				      const char *FileName, bool On)
{
  TRACEF("cXinelibStatusMonitor::Replaying");

  if (On /*&& Name != NULL*/) {
    TRACE("cXinelibStatusMonitor: Replaying " << Name << "(" << FileName << ")");
    m_Device.SetReplayMode();
  }
}
#endif

//----------------------------- device ----------------------------------------

//
// Singleton
//

cXinelibDevice* cXinelibDevice::m_pInstance = NULL;

cXinelibDevice& cXinelibDevice::Instance(void) 
{
  TRACEF("cXinelibDevice::Instance");
  if (!m_pInstance) {
    m_pInstance = new cXinelibDevice();
    TRACE("cXinelibDevice::Instance(): create, cardindex = " 
	  << m_pInstance->CardIndex());
  }
  
  return *m_pInstance;
}

void cXinelibDevice::Dispose(void) 
{
  TRACEF("cXinelibDevice::Dispose");
  delete m_pInstance;
  m_pInstance = NULL;
}

//
// init and shutdown
//

cXinelibDevice::cXinelibDevice()
{
  TRACEF("cXinelibDevice::cXinelibDevice");

  m_statusMonitor = NULL;
  m_spuDecoder    = NULL;

  m_local  = NULL;
  m_server = NULL;

  m_OriginalPrimaryDevice = 0;
  m_ForcePrimaryDeviceCnt = 0;

  if(*xc.local_frontend && strncmp(xc.local_frontend, "none", 4))
    m_clients.Add(m_local = new cXinelibLocal(xc.local_frontend));
  if(xc.remote_mode && xc.listen_port>0)
    m_clients.Add(m_server = new cXinelibServer(xc.listen_port));

  m_ac3Present  = false;
  m_spuPresent  = false;

  m_CurrentDvdSpuTrack = -1;
  ClrAvailableDvdSpuTracks();

  memset(m_MetaInfo, 0, sizeof(m_MetaInfo));
    
  m_PlayMode = pmNone;
  m_LastTrack = ttAudioFirst;
  m_AudioChannel = 0;

  m_liveMode    = false;
  m_TrickSpeed  = -1;
  m_SkipAudio   = false;
  m_PlayingFile = false;
  m_StreamStart = true;
  m_RadioStream = false;
  m_AudioCount  = 0;
  m_Polled = false;
}

cXinelibDevice::~cXinelibDevice() 
{
  TRACEF("cXinelibDevice::~cXinelibDevice");

  StopDevice();

  m_pInstance = NULL;
}

bool cXinelibDevice::StartDevice()
{
  TRACEF("cXinelibDevice::StartDevice");

  // if(dynamic_cast<cXinelibLocal*>(it))
  if(m_local) {
    m_local->Start();
    while(!m_local->IsReady()) {
      cCondWait::SleepMs(100);
      if(m_local->IsFinished()) {
        LOGMSG("cXinelibDevice::Start(): Local frontend init failed");
        return false;
      }
    }
    if(xc.force_primary_device)
      ForcePrimaryDevice(true);
  }

  if(m_server) {
    m_server->Start();
    while(!m_server->IsReady()) {
      cCondWait::SleepMs(100);
      if(m_server->IsFinished()) {
        LOGMSG("cXinelibDevice::Start(): Server init failed");
        return false;
      }
    }
  }

  ASSERT(m_statusMonitor == NULL, false);
  m_statusMonitor = new cXinelibStatusMonitor(*this, CardIndex());

  LOGDBG("cXinelibDevice::StartDevice(): Device started");
  return true;
}

void cXinelibDevice::StopDevice(void) 
{
  TRACEF("cXinelibDevice::StopDevice");
  LOGDBG("cXinelibDevice::StopDevice(): Stopping device ...");

  if(m_statusMonitor) {
    delete m_statusMonitor;
    m_statusMonitor = NULL;
  }
  if (m_spuDecoder) {
    delete m_spuDecoder;
    m_spuDecoder = NULL;
  }

  ForEach(m_clients, &cXinelibThread::SetLiveMode, false);
  TrickSpeed(-1);
  ForEach(m_clients, &cXinelibThread::Stop);
 
  m_local = m_server = NULL;
  m_clients.Clear();
}

//
// Primary device switching
//

void cXinelibDevice::MakePrimaryDevice(bool On) 
{
  TRACEF("cXinelibDevice::MakePrimaryDevice");

  if(On)
    new cXinelibOsdProvider(this);
}

void cXinelibDevice::ForcePrimaryDevice(bool On) 
{
  TRACEF("cXinelibDevice::ForcePrimaryDevice");

  m_MainThreadLock.Lock();
  m_MainThreadFunctors.Add(CreateFunctor(this, &cXinelibDevice::ForcePrimaryDeviceImpl, On));
  m_MainThreadLock.Unlock();
}

void cXinelibDevice::ForcePrimaryDeviceImpl(bool On) 
{
  TRACEF("cXinelibDevice::ForcePrimaryDeviceImpl");
  ASSERT(cThread::IsMainThread(), false);

  if(On) {
    m_ForcePrimaryDeviceCnt++;

    if(xc.force_primary_device) {
      if(cDevice::PrimaryDevice() && this != cDevice::PrimaryDevice()) {
	m_OriginalPrimaryDevice = cDevice::PrimaryDevice()->DeviceNumber() + 1;
	cControl::Shutdown();
	LOGMSG("Forcing primary device, original index = %d", m_OriginalPrimaryDevice);
	if(cOsd::IsOpen()) {
	  LOGMSG("Forcing primary device, old OSD still open !");
#if VDRVERSNUM >= 10400
	  xc.main_menu_mode = CloseOsd;
	  cRemote::CallPlugin("xineliboutput");
#endif
	}
	SetPrimaryDevice(DeviceNumber() + 1);
      }
    }
  
  } else /* Off */ {
    m_ForcePrimaryDeviceCnt--;

    if(m_ForcePrimaryDeviceCnt < 0)
      LOGMSG("ForcePrimaryDevice: Internal error (ForcePrimaryDevice < 0)");
    else if(m_ForcePrimaryDeviceCnt == 0) {
      if(m_OriginalPrimaryDevice) {
	LOGMSG("Restoring original primary device %d", m_OriginalPrimaryDevice);
	cControl::Shutdown();
	if(cOsd::IsOpen()) {
	  LOGMSG("Restoring primary device, xineliboutput OSD still open !");
#if VDRVERSNUM >= 10400
	  xc.main_menu_mode = CloseOsd; /* will be executed in future by vdr main thread */
	  cRemote::CallPlugin("xineliboutput");
#endif
	}
	cChannel *channel = Channels.GetByNumber(CurrentChannel());
	cDevice::SetPrimaryDevice(m_OriginalPrimaryDevice);
	PrimaryDevice()->SwitchChannel(channel, true);
	m_OriginalPrimaryDevice = 0;
      }
    }
  }
}

//
// Execute functors in main thread context
//

void cXinelibDevice::MainThreadHook(void)
{
  TRACEF("cXinelibDevice::MainThreadHook");

  cFunctor *f = NULL;
  do {
    m_MainThreadLock.Lock();
    if(f)
      m_MainThreadFunctors.Del(f);
    f = m_MainThreadFunctors.First();
    m_MainThreadLock.Unlock();

    if(f) {
      /*LOGDBG("cXinelibDevice::MainThreadHook: executing functor 0x%lx",(long)f);*/
      f->Execute();
    }

  } while(f);
}

//
// Configuration
//

void cXinelibDevice::ConfigureOSD(bool prescale_osd, bool unscaled_osd)
{
  TRACEF("cXinelibDevice::ConfigureOSD");

  if(m_local)
    m_local->ConfigureOSD(prescale_osd, unscaled_osd);
  if(m_server)
    m_server->ConfigureOSD(prescale_osd, unscaled_osd);
}

void cXinelibDevice::ConfigurePostprocessing(const char *deinterlace_method, 
					     int audio_delay, 
					     int audio_compression, 
					     const int *audio_equalizer, 
					     int audio_surround,
					     int speaker_type)
{
  TRACEF("cXinelibDevice::ConfigurePostprocessing");

  if(m_local)
    m_local->ConfigurePostprocessing(deinterlace_method, audio_delay, 
				     audio_compression, audio_equalizer,
				     audio_surround, speaker_type);
  if(m_server)
    m_server->ConfigurePostprocessing(deinterlace_method, audio_delay, 
				      audio_compression, audio_equalizer,
				      audio_surround, speaker_type);
}

void cXinelibDevice::ConfigurePostprocessing(const char *name, bool on, 
					     const char *args)
{
  TRACEF("cXinelibDevice::ConfigurePostprocessing");

  if(m_local)
    m_local->ConfigurePostprocessing(name, on, args);
  if(m_server)
    m_server->ConfigurePostprocessing(name, on, args);
}

void cXinelibDevice::ConfigureVideo(int hue, int saturation, int brightness, int contrast,
				    int overscan)
{
  TRACEF("cXinelibDevice::ConfigureVideo");

  if(m_local)
    m_local->ConfigureVideo(hue, saturation, brightness, contrast, overscan);
  if(m_server)
    m_server->ConfigureVideo(hue, saturation, brightness, contrast, overscan);
}

void cXinelibDevice::ConfigureDecoder(int pes_buffers, int priority)
{
  TRACEF("cXinelibDevice::ConfigureDecoder");

  if(m_local)
    m_local->ConfigureDecoder(pes_buffers, priority);
  //if(m_server)
  //  m_server->ConfigureDecoder(pes_buffers, priority);

  cXinelibOsdProvider::RefreshOsd();
}

void cXinelibDevice::ConfigureWindow(int fullscreen, int width, int height, 
				     int modeswitch, const char *modeline, 
				     int aspect, int scale_video, 
				     int field_order)
{
  TRACEF("cXinelibDevice::ConfigureWindow");

  if((!*xc.local_frontend || !strncmp(xc.local_frontend, "none", 4)) && m_local) {
    cXinelibThread *tmp = m_local;
    m_clients.Del(tmp, false);
    m_local = NULL;
    cCondWait::SleepMs(5);
    tmp->Stop();
    cCondWait::SleepMs(5);
    delete tmp;
    if(xc.force_primary_device)
      ForcePrimaryDevice(false);
  }

  if(m_local)
    m_local->ConfigureWindow(fullscreen, width, height, modeswitch, modeline, 
			     aspect, scale_video, field_order);

  else if(*xc.local_frontend && strncmp(xc.local_frontend, "none", 4)) {
    cXinelibThread *tmp = new cXinelibLocal(xc.local_frontend);
    tmp->Start();
    m_clients.Add(m_local = tmp);

    cCondWait::SleepMs(25);
    while(!m_local->IsReady() && !m_local->IsFinished())
      cCondWait::SleepMs(25);

    if(m_local->IsFinished()) {
      m_local = NULL;
      m_clients.Del(tmp, true);
      Skins.QueueMessage(mtError, tr("Frontend initialization failed"), 10);
    } else {
      if(xc.force_primary_device)
	ForcePrimaryDevice(true);
  
      m_local->ConfigureWindow(fullscreen, width, height, modeswitch, modeline,
			       aspect, scale_video, field_order);
    }
  }
}

void cXinelibDevice::Listen(bool activate, int port)
{
  TRACEF("cXinelibDevice::Listen");

  if(activate && port>0) {
    if(!m_server) {
      cXinelibThread *tmp = new cXinelibServer(port);
      tmp->Start();
      m_clients.Add(m_server = tmp);

      cCondWait::SleepMs(10);
      while(!m_server->IsReady() && !m_server->IsFinished())
	cCondWait::SleepMs(10);

      if(m_server->IsFinished()) {
	Skins.QueueMessage(mtError, tr("Server initialization failed"), 10);
	m_server = NULL;
	m_clients.Del(tmp, true);
      }

    } else {
      if(! m_server->Listen(port))
	Skins.QueueMessage(mtError, tr("Server initialization failed"), 10);
    }
  } else if( /*((!activate) || port<=0) && */ m_server) {
    cXinelibThread *tmp = m_server;
    m_clients.Del(tmp, false);
    m_server = NULL;
    cCondWait::SleepMs(5);
    tmp->Stop();
    cCondWait::SleepMs(5);
    delete tmp;
  }
}

//
// OSD
//

void cXinelibDevice::OsdCmd(void *cmd)
{
  TRACEF("cXinelibDevice::OsdCmd");
  TRACK_TIME(250);

  if(m_server)    // call first server, local frontend modifies contents of the message ...
    m_server->OsdCmd(cmd);
  if(m_local)
    m_local->OsdCmd(cmd);
}

//
// Play mode control
//

void cXinelibDevice::StopOutput(void)
{
  TRACEF("cXinelibDevice::StopOutput");
  TRACK_TIME(250);

  m_RadioStream = false;
  m_AudioCount  = 0;

  ForEach(m_clients, &cXinelibThread::SetLiveMode, false);
  Clear();
  ForEach(m_clients, &cXinelibThread::QueueBlankDisplay);
  ForEach(m_clients, &cXinelibThread::SetNoVideo, false);
  ClrAvailableDvdSpuTracks();
}

void cXinelibDevice::SetTvMode(cChannel *Channel)
{
  TRACEF("cXinelibDevice::SetTvMode");
  TRACK_TIME(250);

  m_RadioStream = false;
  if (Channel && !Channel->Vpid() && (Channel->Apid(0) || Channel->Apid(1)))
    m_RadioStream = true;
  if(m_PlayMode == pmAudioOnlyBlack)
    m_RadioStream = true;
  TRACE("cXinelibDevice::SetTvMode - isRadio = "<<m_RadioStream);

  m_StreamStart = true;
  m_liveMode = true;
  m_TrickSpeed = -1;
  m_SkipAudio  = false;
  m_AudioCount = 0;
  m_spuPresent = false;

  Clear();
  ForEach(m_clients, &cXinelibThread::SetNoVideo, m_RadioStream);
  ForEach(m_clients, &cXinelibThread::SetLiveMode, true);
  ForEach(m_clients, &cXinelibThread::ResumeOutput);
}

void cXinelibDevice::SetReplayMode(void)
{
  TRACEF("cXinelibDevice::SetReplayMode");

  m_RadioStream = true; // first seen replayed video packet resets this
  m_AudioCount  = 15;
  m_StreamStart = true;

  ForEach(m_clients, &cXinelibThread::SetLiveMode, false);
  TrickSpeed(-1);
  ForEach(m_clients, &cXinelibThread::Clear);
  ForEach(m_clients, &cXinelibThread::SetNoVideo, false /*m_RadioStream*/);
  if(m_RadioStream && !m_liveMode)
    ForEach(m_clients, &cXinelibThread::BlankDisplay);
  ForEach(m_clients, &cXinelibThread::ResumeOutput);

  m_liveMode = false;
}

bool cXinelibDevice::SetPlayMode(ePlayMode PlayMode) 
{
  TRACEF("cXinelibDevice::SetPlayMode");

#ifdef XINELIBOUTPUT_DEBUG
  switch (PlayMode) {
    case pmNone:     
      TRACE("cXinelibDevice::SetPlayMode audio/video from decoder"); break;
    case pmAudioVideo: 
      TRACE("cXinelibDevice::SetPlayMode audio/video from player"); break;
    case pmVideoOnly:  
      TRACE("cXinelibDevice::SetPlayMode video from player, audio from decoder"); break;
    case pmAudioOnly:  
      TRACE("cXinelibDevice::SetPlayMode audio from player, video from decoder"); break;
    case pmAudioOnlyBlack: 
      TRACE("cXinelibDevice::SetPlayMode audio only from player, no video (black screen)"); break;
    case pmExtern_THIS_SHOULD_BE_AVOIDED: 
      TRACE("cXinelibDevice::SetPlayMode this should be avoided"); break;
  }
#endif

  m_ac3Present = false;
  m_spuPresent = false;

  ClrAvailableDvdSpuTracks();
  m_PlayMode = PlayMode;

  TrickSpeed(-1);
  if (m_PlayMode == pmAudioOnlyBlack) {
    TRACE("pmAudioOnlyBlack --> BlankDisplay, NoVideo");
    ForEach(m_clients, &cXinelibThread::BlankDisplay);
    ForEach(m_clients, &cXinelibThread::SetNoVideo, true);
  } else {
    ForEach(m_clients, &cXinelibThread::SetNoVideo, m_RadioStream && (!m_liveMode || m_AudioCount<1));
    ForEach(m_clients, &cXinelibThread::Clear);
  }
  
  return true;
}

//
// Playback control
//

void cXinelibDevice::TrickSpeed(int Speed) 
{
  TRACEF("cXinelibDevice::TrickSpeed");

  int RealSpeed = abs(Speed);
  m_TrickSpeed = Speed;
  m_TrickSpeedPts = 0;

  ForEach(m_clients, &cXinelibThread::TrickSpeed, RealSpeed);
}

void cXinelibDevice::Clear(void) 
{
  TRACEF("cXinelibDevice::Clear");
  TRACK_TIME(100);

  m_StreamStart = true;
  TrickSpeed(-1);
  ForEach(m_clients, &cXinelibThread::Clear);
}

void cXinelibDevice::Play(void) 
{
  TRACEF("cXinelibDevice::Play");

  m_SkipAudio  = false;
  
  ForEach(m_clients, &cXinelibThread::SetLiveMode, false);
  TrickSpeed(-1);
}

void cXinelibDevice::Freeze(void) 
{
  TRACEF("cXinelibDevice::Freeze");

  TrickSpeed(0);
}

int64_t cXinelibDevice::GetSTC(void)
{
  TRACEF("cXinelibDevice::GetSTC");

  if(m_local)
    return m_local->GetSTC();
  if(m_server)
    return m_server->GetSTC();
  return cDevice::GetSTC();
}

bool cXinelibDevice::Flush(int TimeoutMs) 
{
  TRACEF("cXinelibDevice::Flush");
  TRACK_TIME(500);

  if(m_TrickSpeed == 0) {
    ForEach(m_clients, &cXinelibThread::SetLiveMode, false);
    TrickSpeed(-1);
  }

  bool r = ForEach(m_clients, &cXinelibThread::Flush, TimeoutMs, 
		   &mand<bool>, true);

  return r;
}


//
// Playback of files and images
//

int cXinelibDevice::PlayFileCtrl(const char *Cmd)
{
  TRACEF("cXinelibDevice::PlayFile");
  int result = -1;

  if(m_PlayingFile) {
    if(m_server)
      result = m_server->PlayFileCtrl(Cmd);
    if(m_local) 
      result = m_local->PlayFileCtrl(Cmd);
  }
  return result;
}

bool cXinelibDevice::EndOfStreamReached(void)
{
  if(m_local && !m_local->EndOfStreamReached())
    return false;
  if(m_server && !m_server->EndOfStreamReached())
    return false;
  return true;
}

bool cXinelibDevice::PlayFile(const char *FileName, int Position, bool LoopPlay)
{
  TRACEF("cXinelibDevice::PlayFile");
  TRACE("cXinelibDevice::PlayFile(\"" << FileName << "\")");

  bool result = true;

  if(FileName) {
    if(!m_PlayingFile) {
      m_PlayingFile = true;
      StopOutput();
    }
    if(m_server)
      result = m_server->PlayFile(FileName, Position, LoopPlay);
    if(m_local)
      result = m_local->PlayFile(FileName, Position, LoopPlay);
  } else if(/*!FileName &&*/m_PlayingFile) {
    if(m_server) 
      result = m_server->PlayFile(NULL, 0);
    if(m_local)
      result = m_local->PlayFile(NULL, 0);
    if(!m_liveMode)
      SetReplayMode();
    else
      SetTvMode(Channels.GetByNumber(cDevice::CurrentChannel()));
    m_PlayingFile = false;
  }

  return result;
}

//
// Data stream handling
//
int cXinelibDevice::PlayAny(const uchar *buf, int length) 
{
  TRACEF("cXinelibDevice::PlayAny");
  TRACK_TIME(100);

  if(m_PlayingFile)
    return length;

  //
  // Need to be sure Poll has been called for every frame:
  //  - cDevice can feed multiple frames after each poll from player/transfer.
  //  - If only part of frames are consumed, rest are fed again after next Poll.
  //  - If there are multiple clients it is possible first client(s)
  //    can queue more frames than last client(s).
  //    -> frame(s) are either lost immediately (last client(s)) 
  //       or duplicated after next poll (first client(s))
  //
  if(!m_Polled) {
    /*#warning TODO: modify poll to return count of free bufs and store min() here ? */
    cPoller Poller;
    if(!Poll(Poller,0)) {
      errno = EAGAIN;
      return 0;
    }
  }
  m_Polled = false;

  bool isMpeg1 = false;

  int len = pes_packet_len(buf, length, isMpeg1);
  if(len>0 && len != length) 
    LOGMSG("cXinelibDevice::PlayAny: invalid data !");

  // strip timestamps in trick speed modes
  if(m_SkipAudio || m_TrickSpeed > 0) {
    if(!m_SkipAudio) {

#ifdef TEST_TRICKSPEEDS
# warning Experimental trickspeed mode handling included !
      // TODO: re-gen pts or signal pts+trickspeed for udp scheduler
      bool Video = false, Audio = false;
      uchar PictureType = NO_PICTURE;
      int64_t pts = pes_extract_pts(buf, length, Audio, Video);
      if(m_TrickSpeedPts <= 0LL) {
	if(pts>0 && Video) {
	  m_TrickSpeedPts = pts;
	  if(ScanVideoPacket(buf, length, PictureType) > 0)
	    ;
	  LOGMSG("TrickSpeed: VIDEO PTS %" PRId64 " (%s)", pts,
		 PictureTypeStr(PictureType));
	}
      } else if(Audio) {
	LOGMSG("TrickSpeed: AUDIO PTS %" PRId64, pts);
      } else if(pts > 0LL) {
	if(ScanVideoPacket(buf, length, PictureType) > 0)
	  ;
	LOGMSG("TrickSpeed: VIDEO PTS DIFF %" PRId64 " (%s)", pts - m_TrickSpeedPts,
	       PictureTypeStr(PictureType));
	//m_TrickSpeedPts += (int64_t)(40*90 * m_TrickSpeed); /* 40ms * 90kHz */
	//pes_change_pts((uchar *)buf, length);
      }
      pes_strip_pts((uchar*)buf, length);

#else
      pes_strip_pts((uchar*)buf, length);
#endif
    } else {
      pes_strip_pts((uchar*)buf, length);
    }
  }

  if(m_local) {
    length = (isMpeg1 ? m_local->Play_Mpeg1_PES(buf,length) : 
	                m_local->Play_PES(buf,length));
  } 
  if(m_server && length > 0) {
    int length2 = isMpeg1 ? m_server->Play_Mpeg1_PES(buf, length) : 
                            m_server->Play_PES(buf, length);   
    if(!m_local)
      return length2;
  }
  
  return length;
}

int cXinelibDevice::PlayVideo(const uchar *buf, int length) 
{
  TRACEF("cXinelibDevice::PlayVideo");

  if(m_PlayMode == pmAudioOnlyBlack)
    return length;

  if(m_RadioStream) {
    m_RadioStream = false;
    m_AudioCount  = 0;
    ForEach(m_clients, &cXinelibThread::SetNoVideo, m_RadioStream);
  }
  
  if(m_StreamStart) {

#ifdef START_IFRAME
    // Start with I-frame if stream has video
    // wait for first I-frame
    uchar pictureType;
    if( ScanVideoPacket(buf, length, /*0,*/pictureType) > 0 && 
	pictureType == I_FRAME) {
      m_StreamStart = false;
    } else {
      return length;
    }
#endif

#if defined(HD_MODE_TEST)
    int Width, Height;
    if(GetVideoSize(buf, length, &Width, &Height)) {
      m_StreamStart = false;
      //LOGDBG("Detected video size %dx%d", Width, Height);
      ForEach(m_clients, &cXinelibThread::SetHDMode, (Width > 800));
    }
#endif
  }

#ifdef DEBUG_SWITCHING_TIME
  if(m_statusMonitor->switchtimeOff && m_statusMonitor->switchtimeOn) {
    uchar pictureType;
    if( ScanVideoPacket(buf, length, pictureType) > 0 && 
	pictureType == I_FRAME)
      m_statusMonitor->IFrame();
  }
#endif

  return PlayAny(buf, length);
}

void cXinelibDevice::StillPicture(const uchar *Data, int Length) 
{
  TRACEF("cXinelibDevice::StillPicture");

  bool isPes   = (!Data[0] && !Data[1] && Data[2] == 0x01 && 
		  (Data[3] & 0xF0) == 0xE0);
  bool isMpeg1 = isPes && ((Data[6] & 0xC0) != 0x80);
  int i;

  if(m_PlayingFile)
    return;

  TRACE("cXinelibDevice::StillPicture: isPes = "<<isPes
	<<", isMpeg1 = "<<isMpeg1);

  ForEach(m_clients, &cXinelibThread::Clear);
  ForEach(m_clients, &cXinelibThread::SetNoVideo, false);
  ForEach(m_clients, &cXinelibThread::SetLiveMode, false);
  ForEach(m_clients, &cXinelibThread::SetStillMode, true);
  ForEach(m_clients, &cXinelibThread::TrickSpeed, 1);

  m_TrickSpeed = -1; // to make Poll work ...
  m_SkipAudio = 1;   // enables audio and pts stripping

  for(i=0; i<STILLPICTURE_REPEAT_COUNT; i++)
    if(isMpeg1) {
      ForEach(m_clients, &cXinelibThread::Play_Mpeg1_PES, Data, Length, 
	      &mmin<int>, Length);
    } else if(isPes) {
      /*cDevice::*/PlayPes(Data, Length, m_SkipAudio);
    } else {
      ForEach(m_clients, &cXinelibThread::Play_Mpeg2_ES, 
	      Data, Length, VIDEO_STREAM,
	      &mand<bool>, true);
    }

  ForEach(m_clients, &cXinelibThread::Play_Mpeg2_ES,
	  Data, 0, VIDEO_STREAM,
	  &mand<bool>, true);

  m_TrickSpeed = 0;
  m_SkipAudio = 0;
}

int cXinelibDevice::PlayAudio(const uchar *buf, int length, uchar Id) 
{
  TRACEF("cXinelibDevice::PlayAudio");

#ifdef SKIP_AC3_AUDIO
    // skip AC3 audio
  if(((unsigned char *)buf)[3] == PRIVATE_STREAM1) {
    TRACE("cXinelibDevice::PlayVideo: PRIVATE_STREAM1 discarded");
    return length;
  }
#endif

  // strip audio in trick speed modes
  if(m_SkipAudio || m_TrickSpeed > 0)
    return length;

  if(m_RadioStream) {
    if(m_AudioCount) {
      m_AudioCount--;
      if(!m_AudioCount) 
	ForEach(m_clients, &cXinelibThread::SetNoVideo, m_RadioStream);
    }
  }

  return PlayAny(buf, length);
}

int cXinelibDevice::PlaySpu(const uchar *buf, int length, uchar Id) 
{
  TRACEF("cXinelibDevice::PlaySpu");

#ifdef SKIP_DVDSPU
  return length;
#else
  if(((unsigned char *)buf)[3] == PRIVATE_STREAM1) {

    if(!m_spuPresent) {
      TRACE("cXinelibDevice::PlaySpu first DVD SPU frame");
      Skins.QueueMessage(mtInfo,"DVD Subtitles");
      m_spuPresent = true;
      
      ForEach(m_clients, &cXinelibThread::SpuStreamChanged, (int)Id);
    }

    // Strip all but selected SPU track
    if(Id != m_CurrentDvdSpuTrack)
      return length;
  }

  return PlayAny(buf, length);
#endif
}

bool cXinelibDevice::Poll(cPoller &Poller, int TimeoutMs) 
{
  TRACEF("cXinelibDevice::Poll");
  TRACK_TIME(400);

  if(m_PlayingFile)
    return true;

  if(m_TrickSpeed == 0) {
    cCondWait::SleepMs(TimeoutMs);
    return Poller.Poll(0);
  }

  if(!m_local && !m_server) {
    /* nothing to do... why do I exist ... ? */
    cCondWait::SleepMs(TimeoutMs);
    return Poller.Poll(0);
  }

  bool result = true;

  if(m_local)
    result = result && m_local->Poll(Poller, TimeoutMs);
  if(m_server)
    result = result && m_server->Poll(Poller, TimeoutMs);

  m_Polled = true;

  return result /*|| Poller.Poll(0)*/;
}

//
// Audio facilities
//

void cXinelibDevice::SetVolumeDevice(int Volume) 
{
  TRACEF("cXinelibDevice::SetVolumeDevice");

  ForEach(m_clients, &cXinelibThread::SetVolume, Volume);  
}

void cXinelibDevice::SetAudioTrackDevice(eTrackType Type)
{
  TRACEF("cXinelibDevice::SetAudioTrackDevice");

  // track changes are autodetected at xine side
}

void cXinelibDevice::SetAudioChannelDevice(int AudioChannel)
{
  TRACEF("cXinelibDevice::SetAudioChannelDevice");

  if(m_AudioChannel != AudioChannel) {

    m_AudioChannel = AudioChannel;

    switch(AudioChannel) {
      default:
      case 0: ConfigurePostprocessing("audiochannel", false, NULL); 
              break;
      case 1: ConfigurePostprocessing("audiochannel", true, "channel=0");
	      break;
      case 2: ConfigurePostprocessing("audiochannel", true, "channel=1");
	      break;
    }
  }
}

void cXinelibDevice::SetDigitalAudioDevice(bool On)
{
  TRACEF("cXinelibDevice::SetDigitalAudioDevice");

  // track changes are autodetected at xine side
}

//
// Video format facilities
//

void cXinelibDevice::SetVideoFormat(bool VideoFormat16_9) 
{
  TRACEF("cXinelibDevice::SetVideoFormat");
  cDevice::SetVideoFormat(VideoFormat16_9);

#if 0
  //
  // TODO
  //
  if(xc.aspect != ASPECT_AUTO && 
     xc.aspect != ASPECT_DEFAULT) {
    if(VideoFormat16_9)
      xc.aspect = ASPECT_16_9;
    else if(xc.aspect == ASPECT_16_9)
      xc.aspect = ASPECT_4_3;
    ConfigureDecoder(,,,xc.aspect,,,);
  }
#endif
}

void cXinelibDevice::SetVideoDisplayFormat(eVideoDisplayFormat VideoDisplayFormat)
{
  TRACEF("cXinelibDevice::SetVideoDisplayFormat");
  cDevice::SetVideoDisplayFormat(VideoDisplayFormat);

#if 0
  //
  // TODO
  //
  //  - set normal, pan&scan, letterbox (only for 4:3?)
  //
  if(xc.aspect != ASPECT_AUTO && 
     xc.aspect != ASPECT_DEFAULT)  {
    switch(VideoDisplayFormat) {
    case vdfPanAndScan:
      xc.aspect = ASPECT_PAN_SCAN;
      break;
    case vdfLetterBox:    
      xc.aspect = ASPECT_4_3; /* borders are added automatically if needed */
      break;
    case vdfCenterCutOut:
      xc.aspect = ASPECT_CENTER_CUT_OUT;
      break;
    }
    ConfigureDecoder(,,,xc.aspect,,,);
  }
#endif
}

eVideoSystem cXinelibDevice::GetVideoSystem(void)
{
  TRACEF("cXinelibDevice::GetVideoSystem");
  return cDevice::GetVideoSystem();
}


//
// SPU decoder
//

#ifdef FORWARD_DVD_SPUS
# include "spu_decoder.h"
#endif

cSpuDecoder *cXinelibDevice::GetSpuDecoder(void)
{
  TRACEF("cXinelibDevice::GetSpuDecoder");
  if (!m_spuDecoder && IsPrimaryDevice()) {
    //
    // TODO
    //
    //  - use own derived SpuDecoder with special cXinelibOsd 
    //    -> always visible
    //

#ifdef FORWARD_DVD_SPUS
    // forward DVD SPUs to xine without decoding
    m_spuDecoder = new cFwdSpuDecoder(this);
#else
    m_spuDecoder = new cDvbSpuDecoder();
#endif
  }
  return m_spuDecoder;
}

//
// Image Grabbing
//

#if VDRVERSNUM < 10338

bool cXinelibDevice::GrabImage(const char *FileName, bool Jpeg, 
			       int Quality, int SizeX, int SizeY)
{
  uchar *Data = NULL;
  int Size = 0;
  TRACEF("cXinelibDevice::GrabImage");

  if(m_local)
    Data = m_local->GrabImage(Size, Jpeg, Quality, SizeX, SizeY);
  if(!Data && m_server)
    Data = m_local->GrabImage(Size, Jpeg, Quality, SizeX, SizeY);

  if(Data) {
    FILE *fp = fopen(FileName, "wb");
    if(fp) {
      fwrite(Data, Size, 1, fp);
      fclose(fp);
      free(Data);     
      return true;
    } 
    LOGERR("Grab: Can't open %s", FileName);
    free(Data);
  } else {
    LOGMSG("Grab to %s failed", FileName);
  }
  return false;
}

#else

uchar *cXinelibDevice::GrabImage(int &Size, bool Jpeg, 
				 int Quality, int SizeX, int SizeY)
{
  TRACEF("cXinelibDevice::GrabImage");

  if(m_local)
    return m_local->GrabImage(Size, Jpeg, Quality, SizeX, SizeY);
  if(m_server)
    return m_server->GrabImage(Size, Jpeg, Quality, SizeX, SizeY);

  return NULL;
}

#endif


//
// DVD SPU support in VDR recordings
//
//   - override cDevice::PlayPesPacket to get DVD SPUs
//

int cXinelibDevice::PlayPesPacket(const uchar *Data, int Length, 
				  bool VideoOnly)
{
#ifndef SKIP_DVDSPU
  switch (Data[3]) {
    case 0xBD: { // private stream 1
      int PayloadOffset = Data[8] + 9;
      uchar SubStreamId = Data[PayloadOffset];
      uchar SubStreamType = SubStreamId & 0xF0;
      uchar SubStreamIndex = SubStreamId & 0x1F;
      switch (SubStreamType) {
        case 0x20: // SPU
        case 0x30: // SPU
	  SetAvailableDvdSpuTrack(SubStreamIndex);
	  return PlaySpu(Data, Length, SubStreamIndex);
	  break;
        default:
	  ;
      }
    }
    default:
      ;
  }
#endif
  return cDevice::PlayPesPacket(Data, Length, VideoOnly);
}


//
// Available DVD SPU tracks
//

bool cXinelibDevice::SetCurrentDvdSpuTrack(int Type)
{
  if(Type == -1 || 
     ( Type >= 0 && 
       Type < 64 &&
       m_DvdSpuTrack[Type].id != 0xffff)) {
    m_CurrentDvdSpuTrack = Type;
    ForEach(m_clients, &cXinelibThread::SpuStreamChanged, Type);
    return true;
  }
  return false;
}

void cXinelibDevice::ClrAvailableDvdSpuTracks(bool NotifyFrontend)
{
  for(int i=0; i<64; i++)
    m_DvdSpuTrack[i].id = 0xffff;
  if(m_CurrentDvdSpuTrack >=0 ) {
    m_CurrentDvdSpuTrack = -1;
    if(NotifyFrontend)
      ForEach(m_clients, &cXinelibThread::SpuStreamChanged, -1);
  }
}

int cXinelibDevice::NumDvdSpuTracks(void) const
{ 
  int DvdSpuTracks = 0;
  for(int i=0; i<64; i++)
    if(m_DvdSpuTrack[i].id != 0xffff)
      DvdSpuTracks++;
  return DvdSpuTracks; 
}

const tTrackId *cXinelibDevice::GetDvdSpuTrack(int Type) const
{
  if(Type >= 0 && Type < 64 &&
     m_DvdSpuTrack[Type].id != 0xffff)
    return &m_DvdSpuTrack[Type];
  return NULL;
}

const char *cXinelibDevice::GetDvdSpuLang(int Type) const
{
  const tTrackId *track = GetDvdSpuTrack(Type);
  if(track)
    return track->language[0] ? track->language : NULL;
  return NULL;
}

bool cXinelibDevice::SetAvailableDvdSpuTrack(int Type, const char *lang, bool Current)
{
  if(Type >= 0 && Type < 64) {
    m_DvdSpuTrack[Type].id = Type;
    m_DvdSpuTrack[Type].language[0] = '\0';
    if(lang) 
      strn0cpy(m_DvdSpuTrack[Type].language, lang, MAXLANGCODE2);
    //m_DvdSpuTracks++;
    if(Current)
      m_CurrentDvdSpuTrack = Type;
    return true;
  }
  return false;
}

//
// Metainfo
//

const char *cXinelibDevice::GetMetaInfo(eMetainfoType Type)
{
  if(Type >= 0 && Type < mi_Count)
    return m_MetaInfo[Type];

  LOGMSG("cXinelibDevice::GetMetaInfo: unknown metainfo type");
  return "";
}

void  cXinelibDevice::SetMetaInfo(eMetainfoType Type, const char *Value)
{
  if(Type >= 0 && Type < mi_Count) {
    /* set to 0 first, so if player is accessing string in middle of 
       copying it will always be 0-terminated (but truncated) */
    memset(m_MetaInfo[Type], 0, sizeof(m_MetaInfo[Type]));
    strncpy(m_MetaInfo[Type], Value, MAX_METAINFO_LEN);

  } else {
    LOGMSG("cXinelibDevice::SetMetaInfo: unknown metainfo type");
  }
}

