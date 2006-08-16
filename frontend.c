/*
 * frontend.c:
 *
 * See the main source file 'xineliboutput.c' for copyright information and
 * how to reach the author.
 *
 * $Id: frontend.c,v 1.10 2006-08-16 21:46:34 phintuka Exp $
 *
 */

#define __STDC_FORMAT_MACROS
#include <inttypes.h>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/ioctl.h>
#include <sys/socket.h>

#include <vdr/config.h>
#include <vdr/tools.h>
#include <vdr/plugin.h>

#include "logdefs.h"
#include "config.h"
#include "frontend.h"

#include "tools/pes.h"
#include "tools/general_remote.h"

//#define LOG_CONTROL_MESSAGES
//#define XINELIBOUTPUT_LOG_KEYS

//----------------------------- cXinelibThread --------------------------------

//
// keyboard control handler
//

/*static*/
void cXinelibThread::KeypressHandler(const char *keymap, const char *key, 
				     bool repeat, bool release)
{
#ifdef XINELIBOUTPUT_LOG_KEYS
  static FILE *flog = fopen("/video/keys.log","w");
  fprintf(flog,"KEY %s %s %d %d\n",keymap,key,repeat,release);fflush(flog);
#endif

  TRACE("keypress_handler: " << (keymap?keymap:"") << " " << key);

  if(!key)
    return;

  if(keymap) {
    cRemote *item = Remotes.First();
    while(item) {
      if(!strcmp(item->Name(), keymap)) {
	// dirty... but only way to support learning ...
	((cGeneralRemote*)item)->Put(key, repeat, release);
	return;
      }
      item = Remotes.Next(item);
    }
    cGeneralRemote *r = new cGeneralRemote(keymap);
    if(*key)
      r->Put(key, repeat, release);
  } else {
    cRemote::Put(cKey::FromString(key));
  }
}

cXinelibThread::cXinelibThread(const char *Description) : cThread(Description)
{
  TRACEF("cXinelibThread::cXinelibThread");

  m_bStopThread = false;
  m_bReady = false;
  m_bIsFinished = false;
  m_bNoVideo = true;
  m_bLiveMode = false;
  m_StreamPos = 0;
  m_Frames = 0;
  m_bEndOfStreamReached = false;
  m_bPlayingFile = false;
  m_FileName = NULL;
}

cXinelibThread::~cXinelibThread() 
{
  TRACEF("cXinelibThread::~cXinelibThread");

  if(Active())
    Cancel();
  if(m_FileName)
    free(m_FileName);
}

//
// Thread control
//

void cXinelibThread::Start(void) 
{
  TRACEF("cXinelibThread::Start");

  cThread::Start(); 
}

void cXinelibThread::Stop(void)  
{ 
  TRACEF("cXinelibThread::Stop");

  SetStopSignal(); 

  //if(Active()) 
  Cancel(5); 
}

void cXinelibThread::SetStopSignal(void) 
{
  TRACEF("cXinelibThread::SetStopSignal");

  LOCK_THREAD;
  m_bStopThread = true;
}

bool cXinelibThread::GetStopSignal(void) 
{
  TRACEF("cXinelibThread::GetStopSignal");

  LOCK_THREAD;
  return m_bStopThread;
}

bool cXinelibThread::IsReady(void)
{
  LOCK_THREAD;
  return m_bReady;
}

bool cXinelibThread::IsFinished(void)
{
  LOCK_THREAD;
  return m_bIsFinished;
}

//
// Playback control
//

void cXinelibThread::SetVolume(int NewVolume)
{
  Xine_Control("VOLUME", NewVolume * 100 / 255);
}

void cXinelibThread::TrickSpeed(int Speed)
{
  TRACEF("cXinelibThread::TrickSpeed");

  Xine_Control("TRICKSPEED", Speed);
}

void cXinelibThread::SetLiveMode(bool LiveModeOn)
{
  TRACEF("cXinelibThread::SetLiveMode");

  Lock();
  if(m_bLiveMode == LiveModeOn) {
    Unlock();
    return;
  }
  m_bLiveMode = LiveModeOn;
  Unlock();

  Xine_Control("LIVE", m_bLiveMode ? 1 : 0);
  Xine_Sync();
}

void cXinelibThread::SetStillMode(bool StillModeOn)
{
  TRACEF("cXinelibThread::SetStillMode");
  Xine_Control("STILL", StillModeOn ? 1 : 0);
  Xine_Sync();
}

void cXinelibThread::SetNoVideo(bool bVal)
{
  TRACEF("cXinelibThread::SetNoVideo");

  Lock();
  if(m_bNoVideo == bVal) {
    Unlock();
    return;
  }
  m_bNoVideo = bVal;
  Unlock();

  Xine_Control("NOVIDEO", m_bNoVideo ? 1 : 0);

  if(m_bNoVideo && strcmp(xc.audio_visualization, "none")) {
    ConfigurePostprocessing(xc.audio_visualization, true, NULL);
  } else {
    ConfigurePostprocessing("AudioVisualization", false, NULL);
  }
}

void cXinelibThread::AudioStreamChanged(bool ac3, int StreamId)
{
  TRACEF("cXinelibThread::AudioStreamChanged");
  if(ac3)
    Xine_Control("AUDIOSTREAM AC3", StreamId);
  else
    Xine_Control("AUDIOSTREAM", StreamId);
}

void cXinelibThread::SpuStreamChanged(int StreamId)
{
  TRACEF("cXinelibThread::SpuStreamChanged");
  Xine_Control("SPUSTREAM", StreamId);
}

void cXinelibThread::Clear(void)
{
  TRACEF("cXinelibThread::Clear");

  Lock();
  int64_t  tmp1 = m_StreamPos;
  uint32_t tmp2 = m_Frames;
  Unlock();
#if 0
  Xine_Control("DISCARD", tmp);
#else
  char buf[128];
  sprintf(buf, "DISCARD %" PRId64 " %d", tmp1, tmp2);
  Xine_Control(buf);
#endif
}

bool cXinelibThread::Flush(int TimeoutMs) 
{
  TRACEF("cXinelibThread::Flush");

  return Xine_Control("FLUSH", TimeoutMs) <= 0;
}

bool cXinelibThread::Poll(cPoller& Poller, int TimeoutMs) 
{
  TRACEF("cXinelibThread::Poll");

  int n = Xine_Control("POLL", TimeoutMs);

  if(n>0)
    return true;

  return false; // Poller.Poll(TimeoutMs);
}

//
// Data transfer
//

int cXinelibThread::Play_PES(const uchar *data, int len)
{
  Lock();
  m_StreamPos += len;
  m_Frames++;
  /*m_bEndOfStreamReached = false;*/
  Unlock();
  return len;
}

//
// Stream conversions
//

// Convert MPEG1 PES headers to MPEG2 PES headers

int cXinelibThread::Play_Mpeg1_PES(const uchar *data1, int len)
{
  if(!data1[0] && !data1[1] && data1[2] == 0x01 && len>7 && /* header sync bytes */
     ( VIDEO_STREAM == (data1[3] & ~VIDEO_STREAM_MASK) ||   /*   video stream */
       AUDIO_STREAM == (data1[3] & ~AUDIO_STREAM_MASK) ||   /*   audio stream */
       PRIVATE_STREAM1 == data1[3]) &&                      /*   private stream 1 */
     ((data1[6] & 0xC0) != 0x80) &&                         /* really mpeg1 pes */
     (len == ((data1[4]<<8) | data1[5]) + 6)) {             /* whole PES packet and nothing else */
    uchar *data2 = new uchar[len+64];
    int i1=0, i2=0, r=0;
    
    data2[i2++]=data1[i1++];  // 00 (sync)  
    data2[i2++]=data1[i1++];  // 00 (sync)
    data2[i2++]=data1[i1++];  // 01 (sync)
    data2[i2++]=data1[i1++];  // stream ID
    data2[i2++]=data1[i1++];  // len hi
    data2[i2++]=data1[i1++];  // len lo
    
    // skip stuffing
    while ((data1[i1] & 0x80) == 0x80) 
      i1++;

    if ((data1[i1] & 0xc0) == 0x40) {
      // skip STD_buffer_scale, STD_buffer_size
      i1 += 2;
    }
    
    if(len<i1+5) return len;
    
    data2[i2++] = 0x80;
    
    if ((data1[i1] & 0xf0) == 0x20) { 
      /* PTS */
      data2[i2++] = 0x80;
      data2[i2++] = 5;
      data2[i2++] = data1[i1++] & 0x0E;
      data2[i2++] = data1[i1++] & 0xFF;
      data2[i2++] = data1[i1++] & 0xFE;
      data2[i2++] = data1[i1++] & 0xFF;
      data2[i2++] = data1[i1++] & 0xFE;
    }
    else if ((data1[i1] & 0xf0) == 0x30) { 
      /* PTS & DTS */
      data2[i2++] = 0x80|0x40;
      data2[i2++] = 10;
      data2[i2++] = data1[i1++] & 0x0E;
      data2[i2++] = data1[i1++] & 0xFF;
      data2[i2++] = data1[i1++] & 0xFE;
      data2[i2++] = data1[i1++] & 0xFF;
      data2[i2++] = data1[i1++] & 0xFE;
      
      data2[i2++] = data1[i1++] & 0x0E;
      data2[i2++] = data1[i1++] & 0xFF;
      data2[i2++] = data1[i1++] & 0xFE;
      data2[i2++] = data1[i1++] & 0xFF;
      data2[i2++] = data1[i1++] & 0xFE;
    } else {
      i1++;
      data2[i2++] = 0; /* no pts, no dts */
      data2[i2++] = 0; /* header len */
    }
    
    int newlen = ((data1[4]<<8) | data1[5]) + (i2-i1), loops=0;
    data2[4] = ((newlen)&0xff00)>>8;
    data2[5] = ((newlen)&0xff);
    if(len-i1 > 0) {
      memcpy(data2+i2, data1+i1, len-i1);
      cPoller p;
      while(!Poll(p,100) && loops++ < 10) {
	LOGDBG("Play_Mpeg1_PES: poll failed");
      }
      r = Play_PES(data2,newlen+6);
    } 
  
    delete data2;
    return r==newlen+6 ? ((data1[4]<<8)|data1[5])+6 : 0;
  }
  return len; // nothing useful found ...
}

// Pack elementary MPEG stream to PES

bool cXinelibThread::Play_Mpeg2_ES(const uchar *data, int len, int streamID)
{
  static uchar hdr[] = {0x00,0x00,0x01,0xe0, 0x00,0x00,0x80,0,0}; /* mpeg2 */
  int todo = len, done = 0, hdrlen = 9/*sizeof(hdr)*/;
  uchar *frame = new uchar[PES_CHUNK_SIZE+32];

  hdr[3] = (uchar)streamID;
  while(todo) {
    int blocklen = todo;
    if(blocklen > ((PES_CHUNK_SIZE - hdrlen) & 0xfffc))
      blocklen = (PES_CHUNK_SIZE - hdrlen) & 0xfffc;
    hdr[4] = ((blocklen+3)&0xff00)>>8;
    hdr[5] = ((blocklen+3)&0xff);

    memcpy(frame, hdr, hdrlen);
    memcpy(frame+hdrlen, data+done, blocklen);

    done += blocklen;
    todo -= blocklen;

    cPoller p;
#if 1
    Poll(p, 100);
#else
    int loops=0;
    while(!Poll(p,100) && loops++ < 10) {
      LOGDBG("Play_ES: Poll Failed");
    }
#endif

    if(blocklen+hdrlen != Play_PES(frame,blocklen+hdrlen)) {      
      delete frame;
      return false;
    }
  }

  // append sequence end code to video 
  if((streamID & 0xF0) == 0xE0) { 
    static uchar seqend[] = {0x00,0x00,0x01,0xe0, 0x00,0x07,0x80,0x00,
			     0x00,
			     0x00,0x00,0x01,0xB7}; /* mpeg2 */
    seqend[3] = (uchar)streamID;
    Play_PES(seqend, 13);
  }

  delete[] frame;
  return true;
}

//
// Built-in still images
//

bool cXinelibThread::QueueBlankDisplay(void)
{
  TRACEF("cXinelibThread::BlankDisplay");

  extern const unsigned char v_mpg_black[];     // black_720x576.c
  extern const int v_mpg_black_length;

  return Play_Mpeg2_ES(v_mpg_black, v_mpg_black_length, VIDEO_STREAM);
}

bool cXinelibThread::BlankDisplay(void)
{
  TRACEF("cXinelibThread::BlankDisplay");

  bool r = QueueBlankDisplay();
  for(int i=0; i<5 && !Flush(100); i++)
    ;
  return r;
}

bool cXinelibThread::LogoDisplay(void)
{
  TRACEF("cXinelibThread::LogoDisplay");

  extern const unsigned char v_mpg_vdrlogo[];     // vdrlogo_720x576.c
  extern const int v_mpg_vdrlogo_length;

  bool r = Play_Mpeg2_ES(v_mpg_vdrlogo, v_mpg_vdrlogo_length, VIDEO_STREAM);
  for(int i=0; i<5 && !Flush(100); i++)
    ;
  return r;
}

bool cXinelibThread::NoSignalDisplay(void)
{
  TRACEF("cXinelibThread::NoSignalDisplay");

  extern const unsigned char v_mpg_nosignal[];     // nosignal_720x576.c
  extern const int v_mpg_nosignal_length;

  bool r = Play_Mpeg2_ES(v_mpg_nosignal, v_mpg_nosignal_length, VIDEO_STREAM);
  for(int i=0; i<5 && !Flush(100); i++)
    ;
  return r;
}

//
// Xine Control
//

int cXinelibThread::Xine_Control(const char *cmd, int p1)
{
  char buf[128];
  sprintf(buf, "%s %d", cmd, p1);
  return Xine_Control(buf);
}

int cXinelibThread::Xine_Control(const char *cmd, int64_t p1)
{
  char buf[128];
  sprintf(buf, "%s %" PRId64, cmd, p1);
  return Xine_Control(buf);
}

int cXinelibThread::Xine_Control(const char *cmd, const char *p1)
{
  char buf[128];
  sprintf(buf, "%s %s", cmd, p1);
  return Xine_Control(buf);
}

bool cXinelibThread::PlayFile(const char *FileName, int Position, 
			      bool LoopPlay)
{
  TRACEF("cXinelibThread::PlayFile");
  char buf[2048];
  m_bEndOfStreamReached = false;
  sprintf(buf, "PLAYFILE %s %d %s\r\n",
	  LoopPlay ? "Loop" : "", Position, FileName ? FileName : "");
  int result = PlayFileCtrl(buf);

  if(!FileName || result != 0) {
    m_bPlayingFile = false;
    if(m_FileName)
      free(m_FileName);
    m_FileName = NULL;
  } else { 
    if(m_FileName)
      free(m_FileName);
    m_FileName = strdup(FileName);
    m_bPlayingFile = true; 
  }

  return (!GetStopSignal()) && (result==0);
}


//
// Configuration
//

int cXinelibThread::ConfigureOSD(bool prescale_osd, bool unscaled_osd) 
{
  char s[256];
  sprintf(s, "OSDSCALING %d", prescale_osd);
  if(!xc.prescale_osd_downscale)
    strcat(s, " NoDownscale");
  if(unscaled_osd)
    strcat(s, " UnscaledAlways");
  if(xc.unscaled_osd_opaque)
    strcat(s, " UnscaledOpaque");
  if(xc.unscaled_osd_lowresvideo)
    strcat(s, " UnscaledLowRes");

  return Xine_Control(s);
}

int cXinelibThread::ConfigurePostprocessing(const char *deinterlace_method, 
					    int audio_delay, 
					    int audio_compression, 
					    const int *audio_equalizer,
					    int audio_surround) 
{
  char tmp[255];
  int r = true;

  if(strcmp(deinterlace_method, "tvtime")) 
    r = ConfigurePostprocessing("tvtime", false, NULL) && r;

  r = Xine_Control("DEINTERLACE", deinterlace_method) && r;
  r = Xine_Control("AUDIODELAY", audio_delay) && r;
  r = Xine_Control("AUDIOCOMPRESSION", audio_compression) && r;
  r = Xine_Control("AUDIOSURROUND", audio_surround) && r;
  sprintf(tmp,"EQUALIZER %d %d %d %d %d %d %d %d %d %d",
	  audio_equalizer[0],audio_equalizer[1],
	  audio_equalizer[2],audio_equalizer[3],
	  audio_equalizer[4],audio_equalizer[5],
	  audio_equalizer[6],audio_equalizer[7],
	  audio_equalizer[8],audio_equalizer[9]);
  r = Xine_Control(tmp) && r;

  if(m_bNoVideo && strcmp(xc.audio_visualization, "none")) {
    //fe->post_open(fe, xc.audio_visualization, NULL);
    r = ConfigurePostprocessing(xc.audio_visualization, true, NULL) && r;
  } else {
    //fe->post_close(fe, NULL, 0);
    r = ConfigurePostprocessing("AudioVisualization", false, NULL) && r;
  }

  if(!strcmp(deinterlace_method, "tvtime")) 
    r = ConfigurePostprocessing("tvtime", true, xc.deinterlace_opts) && r;

  return r;
}

int cXinelibThread::ConfigurePostprocessing(const char *name, bool on, const char *args)
{
  char tmp[1024];  
  if(on) {
    sprintf(tmp, "POST %s On %s", (name&&*name)?name:"0", args?args:"");
    return Xine_Control(tmp);
  } else {
    // 0 - audio vis.
    // 1 - audio post
    // 2 - video post
    //return fe->post_close(fe, name, -1);
    sprintf(tmp, "POST %s Off", (name&&*name)?name:"0");
    return Xine_Control(tmp);
  }
  return -1;
}

int cXinelibThread::ConfigureVideo(int hue, int saturation, 
				   int brightness, int contrast)
{
  char cmd[128];
  sprintf(cmd, "VIDEO_PROPERTIES %d %d %d %d", 
	  hue, saturation, brightness, contrast);
  return Xine_Control(cmd);
}

//
// Playback files
//

bool cXinelibThread::EndOfStreamReached(void) 
{
  LOCK_THREAD;
  bool r = m_bEndOfStreamReached;
  return r;
}


