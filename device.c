/*
 * device.c: xine-lib output device for the Video Disk Recorder
 *
 * See the main source file 'xineliboutput.c' for copyright information and
 * how to reach the author.
 *
 * $Id: device.c,v 1.22 2006-09-09 23:09:56 phintuka Exp $
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

#ifdef DEBUG_SWITCHING_TIME
int64_t switchtimeOff = 0LL;
int64_t switchtimeOn = 0LL;
bool    switchingIframe;
#endif

class cXinelibStatusMonitor : public cStatus 
{
  private:
    cXinelibStatusMonitor(); 
    cXinelibStatusMonitor(cXinelibStatusMonitor&); 

  public:
    cXinelibStatusMonitor(cXinelibDevice& device, int cardIndex) :
        m_Device(device), m_cardIndex(cardIndex) {};

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

// Singleton
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

  if(*xc.local_frontend && strncmp(xc.local_frontend, "none", 4))
    m_clients.Add(m_local = new cXinelibLocal(xc.local_frontend));
  if(xc.remote_mode && xc.listen_port>0)
    m_clients.Add(m_server = new cXinelibServer(xc.listen_port));

  m_ac3Present  = false;
  m_spuPresent  = false;

  m_CurrentDvdSpuTrack = -1;
  ClrAvailableDvdSpuTracks();

  m_PlayMode = pmNone;
  m_LastTrack = ttAudioFirst;

  m_liveMode    = false;
  m_TrickSpeed  = -1;
  m_SkipAudio   = false;
  m_PlayingFile = false;
  m_StreamStart = true;
  m_RadioStream = false;
  m_AudioCount  = 0;
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
  static int Original = 0;
  static int Counter = 0;

  TRACEF("cXinelibDevice::ForcePrimaryDeviceImpl");
  /*LOGDBG("cXinelibDevice::ForcePrimaryDeviceImpl(%s)",On?"On":"Off");*/

  /* TODO: All this stuff should really be done in VDR main thread context... */

  if(On) {
    Counter++;
    if(xc.force_primary_device) {
      if(cDevice::PrimaryDevice() && this != cDevice::PrimaryDevice()) {
	/* TODO: may need to use vdr main thread for this */
	Original = cDevice::PrimaryDevice()->DeviceNumber() + 1;
	cControl::Shutdown();
	LOGMSG("Forcing primary device, original index = %d", Original);
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
    Counter--;
    if(Counter<0)
      LOGMSG("ForcePrimaryDevice: Internal error (ForcePrimaryDevice < 0)");
    if(!Counter) {
      if(Original) {
	LOGMSG("Restoring original primary device %d", Original);
	cControl::Shutdown();
	if(cOsd::IsOpen()) {
	  LOGMSG("Restoring primary device, xineliboutput OSD still open !");
#if VDRVERSNUM >= 10400
	  xc.main_menu_mode = CloseOsd; /* will be executed in future by vdr main thread */
	  cRemote::CallPlugin("xineliboutput");
#endif
	}
	cChannel *channel = Channels.GetByNumber(CurrentChannel());
	cDevice::SetPrimaryDevice(Original);
	PrimaryDevice()->SwitchChannel(channel, true);
	Original = 0;
      }
    }
  }
}

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
    ForEach(m_clients, &cXinelibThread::SetNoVideo, m_RadioStream);
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

  bool isMpeg1 = false;

  int len = pes_packet_len(buf, length, isMpeg1);
  if(len>0 && len != length) 
    LOGMSG("cXinelibDevice::PlayAny: invalid data !");

  // strip timestamps in trick speed modes
  if(m_SkipAudio || m_TrickSpeed > 0) {
    if(!m_SkipAudio) {

#ifdef TEST_TRICKSPEEDS
#warning Experimental trickspeed mode handling included !
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
  
#ifdef START_IFRAME
  // Start with I-frame if stream has video
  if(m_StreamStart) {
    // wait for first I-frame
    uchar pictureType;
    if( ScanVideoPacket(buf, length, /*0,*/pictureType) > 0 && 
	pictureType == I_FRAME) {
      m_StreamStart = false;
    } else {
      return length;
    }
  }
#elif defined(HD_MODE_TEST)
  if(m_StreamStart) {
    int i = 8;         // the minimum length of the video packet header
    i += buf[i] + 1;   // possible additional header bytes
    for (; i < length-6; i++) {
      if (buf[i] == 0 && buf[i + 1] == 0 && buf[i + 2] == 1) {
	if(buf[i + 3] == 0xb3) {
	  int d = (buf[i+4] << 16) | (buf[i+5] << 8) | buf[i+6];
	  int w = (d >> 12);
	  int h = (d & 0xfff);

	  m_StreamStart = false;
	  LOGMSG("Detected video size %dx%d", w, h);
	  ForEach(m_clients, &cXinelibThread::SetHDMode, (w>800));
	  break;
	}
      }
    }
  }
#else
  m_StreamStart = false;
#endif


#ifdef DEBUG_SWITCHING_TIME
  if(switchtimeOff && switchtimeOn) {
    uchar pictureType;
    if( ScanVideoPacket(buf, length, /*0,*/pictureType) > 0 && 
	pictureType == I_FRAME) {
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
  m_SkipAudio = 1;  // enables audio and pts stripping

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
      Skins.QueueMessage(mtInfo,"DVD SPU");
      m_spuPresent = true;

      ForEach(m_clients, &cXinelibThread::SpuStreamChanged, (int)Id);
    }

    if(Id != m_CurrentDvdSpuTrack)
      return length;
  }
printf("SPU %d\n", Id);

  //
  // TODO: channel must be selectable
  //

  return PlayAny(buf, length);
#endif
}

void cXinelibDevice::SetVolumeDevice(int Volume) 
{
  TRACEF("cXinelibDevice::SetVolumeDevice");

  ForEach(m_clients, &cXinelibThread::SetVolume, Volume);  
}

void cXinelibDevice::SetAudioTrackDevice(eTrackType Type)
{
  TRACEF("cXinelibDevice::SetAudioTrackDevice");

  /*LOGDBG("SetAudioTrackDevice(%d)", (int)Type);*/
#if 0
  if(IS_DOLBY_TRACK(Type))
    ForEach(m_clients, &cXinelibThread::AudioStreamChanged, 
	    true, (int)(Type - ttDolbyFirst));
  if(IS_AUDIO_TRACK(Type))
    ForEach(m_clients, &cXinelibThread::AudioStreamChanged,
	    false, AUDIO_STREAM + (int)(Type - ttAudioFirst));
#endif
}

void cXinelibDevice::SetAudioChannelDevice(int AudioChannel)
{
  TRACEF("cXinelibDevice::SetAudioChannelDevice");

  m_AudioChannel = AudioChannel;

  switch(AudioChannel) {
  default:
  case 0: ConfigurePostprocessing("audiochannel", false, NULL); break;
  case 1: ConfigurePostprocessing("audiochannel", true, "channel=0"); break;
  case 2: ConfigurePostprocessing("audiochannel", true, "channel=1"); break;
  }
}

void cXinelibDevice::SetDigitalAudioDevice(bool On)
{
  TRACEF("cXinelibDevice::SetDigitalAudioDevice");
  /*LOGDBG("SeDigitalAudioDevice(%s)", On ? "on" : "off");*/

  eTrackType CurrTrack = GetCurrentAudioTrack();
  if(m_LastTrack != CurrTrack) {
    bool ac3   = IS_DOLBY_TRACK(CurrTrack);
    int  index = CurrTrack - (ac3 ? ttDolbyFirst : ttAudioFirst);
    m_LastTrack = CurrTrack;
#if 0
    LOGDBG("    Switching audio track -> %d (%02x:%s:%d)", m_LastTrack, 
	   ac3 ? PRIVATE_STREAM1 : (index+AUDIO_STREAM), 
	   ac3 ? "AC3" : "MPEG", index);
#endif
    if(ac3) 
      ForEach(m_clients, &cXinelibThread::AudioStreamChanged, true, 
	      (PRIVATE_STREAM1 << 8) | index);
    else 
      ForEach(m_clients, &cXinelibThread::AudioStreamChanged, false, 
	      (index + AUDIO_STREAM) << 8);
  }
}

void cXinelibDevice::SetVideoFormat(bool VideoFormat16_9) 
{
  TRACEF("cXinelibDevice::SetVideoFormat");
  /*LOGDBG("SetVideoFormat(%s)", VideoFormat16_9 ? "16:9" : "4:3");*/

  cDevice::SetVideoFormat(VideoFormat16_9);

  //
  // TODO
  //
#if 0
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

  /*LOGDBG("SetVideoDisplayFormat(%d)", VideoDisplayFormat);*/
  cDevice::SetVideoDisplayFormat(VideoDisplayFormat);
  //
  // TODO
  //
  //  - set normal, pan&scan, letterbox (only for 4:3?)
  //
#if 0
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

  return result /*|| Poller.Poll(0)*/;
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

#if 0
//
// TODO
//  - forward spu's directly to Xine
//
class cXineSpuDecoder : public cDvbSpuDecoder
{
  private:
    cSpuDecoder::eScaleMode scaleMode;
    cXinelibDevice *m_Device;

  public:
     cXineSpuDecoder(cXinelibDevice *dev) { 
       scaleMode = eSpuNormal; 
       m_Device = dev;
     }
     virtual ~cXineSpuDecoder() {};

     virtual int setTime(uint32_t pts) { return 1; }

     cSpuDecoder::eScaleMode getScaleMode(void) { return scaleMode; }
     virtual void setScaleMode(cSpuDecoder::eScaleMode ScaleMode) 
       { scaleMode = ScaleMode; }
     virtual void setPalette(uint32_t * pal) {};
     virtual void setHighlight(uint16_t sx, uint16_t sy,
			       uint16_t ex, uint16_t ey,
			       uint32_t palette) {};
     virtual void clearHighlight(void) {};
     virtual void Empty(void) {};
     virtual void Hide(void) {};
     virtual void Draw(void) {};
     virtual bool IsVisible(void) { return true; }
     virtual void processSPU(uint32_t pts, uint8_t * buf, 
			     bool AllowedShow = true);
};

#define CMD_SPU_MENU            0x00
#define CMD_SPU_SHOW            0x01
#define CMD_SPU_HIDE            0x02
#define CMD_SPU_SET_PALETTE     0x03
#define CMD_SPU_SET_ALPHA       0x04
#define CMD_SPU_SET_SIZE        0x05
#define CMD_SPU_SET_PXD_OFFSET  0x06
#define CMD_SPU_CHG_COLCON      0x07
#define CMD_SPU_EOF             0xff

#define spuU32(i)  ((spu[i] << 8) + spu[i+1])

void cXineSpuDecoder::processSPU(uint32_t pts, uint8_t * buf, bool AllowedShow)
{
  uchar buf2[65536+8] = {0, 0, 1, PRIVATE_STREAM1, 0, 0, 0x80, 0x80, 5};
  int len = ((buf[0] << 8) | buf[1]);

  if(len+8 < 0xffff) {
    buf2[4]  = ((len+8)<<8) & 0xFF;
    buf2[5]  = ((len+8)) & 0xFF;
  } else {
    // should be able to handle this (but only internally ...)
    LOGMSG("cXineSpuDecoder: SPU bigger than PES packet !");
    buf2[4]  = 0xff;
    buf2[5]  = 0xff;
  }
  buf2[9]  = ((pts>>29) & 0x0E) | 0x21;
  buf2[10] = (pts>>22) & 0xFF; 
  buf2[11] = (pts>>14) & 0xFE;
  buf2[12] = (pts>>7)  & 0xFF;
  buf2[13] = (pts<<1)  & 0xFE;

  memcpy(buf2+14, buf, len);

  m_Device->PlaySpu(buf, len+14, 0);
}
#endif

cSpuDecoder *cXinelibDevice::GetSpuDecoder(void)
{
  TRACEF("cXinelibDevice::GetSpuDecoder");
  if (!m_spuDecoder && IsPrimaryDevice())
    //
    // TODO
    //
    //  - use own derived SpuDecoder with special cXinelibOsd 
    //    -> always visible
    //

#if 1
    m_spuDecoder = new cDvbSpuDecoder();
#else
#warning NON-FUNCTIONAL SPU DECODER SELECTED !!!
    m_spuDecoder = new cXineSpuDecoder(this);
#endif
  return m_spuDecoder;
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


#if 1
// override cDevice to get DVD SPUs
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

bool cXinelibDevice::SetCurrentDvdSpuTrack(int Type)
{
  if(Type == -1 || 
     (Type >= 0 && 
      Type < 64 &&
      m_DvdSpuTrack[Type])) {
    m_CurrentDvdSpuTrack = Type;
    ForEach(m_clients, &cXinelibThread::SpuStreamChanged, Type);
    return true;
  }
  return false;
}

void cXinelibDevice::ClrAvailableDvdSpuTracks(void)
{
  m_DvdSpuTracks = 0;
  for(int i=0; i<64; i++)
    m_DvdSpuTrack[i] = false;
  if(m_CurrentDvdSpuTrack >=0 ) {
    m_CurrentDvdSpuTrack = -1;
    ForEach(m_clients, &cXinelibThread::SpuStreamChanged, -1);
  }
}

bool cXinelibDevice::SetAvailableDvdSpuTrack(int Type)
{
  if(Type >= 0 && Type < 64 &&
     ! m_DvdSpuTrack[Type]) {
    m_DvdSpuTrack[Type] = true;
    m_DvdSpuTracks++;
    return true;
  }
  return false;
}

bool cXinelibDevice::HasDvdSpuTrack(int Type) const 
{
  if(Type >= 0 && Type < 64 &&
     m_DvdSpuTrack[Type])
    return true;
  return false;
}

#endif
