/*
 * udp_pes_scheduler.h: PES scheduler for UDP/RTP streams
 *
 * See the main source file 'xineliboutput.c' for copyright information and
 * how to reach the author.
 *
 * $Id: udp_pes_scheduler.c,v 1.7 2006-07-04 02:34:07 phintuka Exp $
 *
 */

#define __STDC_FORMAT_MACROS
#include <inttypes.h>

#include <stdint.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/time.h>

#include <vdr/config.h>
#include <vdr/tools.h>

#include "../logdefs.h"

#include "udp_buffer.h"
#include "pes.h"

#include "udp_pes_scheduler.h"

#include "../xine_input_vdr_net.h" // frame headers
#include "../config.h"             // rtp address & port


#include <sys/types.h>
#include <linux/unistd.h>
#include <errno.h>
_syscall0(pid_t, gettid)


//----------------------- cTimePts ------------------------------------------

cTimePts::cTimePts(void)
{
  Set();
}

int64_t cTimePts::Now(void)
{
  struct timeval t;

  if (gettimeofday(&t, NULL) == 0) {
    t.tv_sec -= tbegin.tv_sec;
    if(t.tv_usec < tbegin.tv_usec) {
      t.tv_sec--;
      t.tv_usec += 1000000;
    }
    t.tv_usec -= tbegin.tv_usec;

    return (uint64(t.tv_sec)) * 90000LL + 
           (uint64(t.tv_usec)) * 90LL / 1000LL +
           begin;
  }

  return 0;
}

void cTimePts::Set(int64_t Pts)
{
  gettimeofday(&tbegin, NULL);
  begin = Pts;
}

//----------------------- cUdpScheduler -------------------------------------

//#define LOG_RESEND
//#define LOG_SCR

const int MAX_QUEUE_SIZE      = 64;       // ~ 65 ms with typical DVB stream
const int MAX_LIVE_QUEUE_SIZE = (64+32);  // ~ 100 ms with typical DVB stream
const int HARD_LIMIT          = (4*1024); // ~ 40 Mbit/s === 4 Mb/s

// initial burst length after seek (500ms = ~13 video frames)
const int64_t INITIAL_BURST_TIME  = (int64_t)(45000); // pts units (90kHz)

// assume seek when when pts difference between two frames exceeds this (1.5 seconds)
const int64_t JUMP_LIMIT_TIME = (int64_t)(3*90000/2); // pts units (90kHz)

const int RTCP_MIN_INTERVAL = 90000; // max. once in second

typedef enum {
  eScrDetect,
  eScrFromAudio,
  eScrFromPS1,
  eScrFromVideo
} ScrSource_t;

cUdpScheduler::cUdpScheduler()
{

  // Scheduler data

  current_audio_vtime = 0;
  current_video_vtime = 0;

#ifdef LOG_SCR
  data_sent = 0; 
  frames_sent = 0;
  frame_rate = 2000;
  prev_frames = 200;
#endif

  last_delay_time = 0;

  srandom(time(NULL) ^ gettid());
  m_ssrc = random();
  LOGDBG("RTP SSRC: 0x%08x", m_ssrc);
  m_LastRtcpTime = 0;

  // queuing

  int i;
  for(i=0; i<MAX_UDP_HANDLES; i++)
    m_Handles[i] = -1;

  m_BackLog = new cUdpBackLog;

  m_QueueNextSeq = 0;
  m_QueuePending = 0;

  // Thread

  m_Running = 1;

  Start();
}

cUdpScheduler::~cUdpScheduler()
{
  m_Lock.Lock();
  m_Running = 0;
  m_Cond.Broadcast();
  m_Lock.Unlock();

  Cancel(3);

  delete m_BackLog;
}

bool cUdpScheduler::AddHandle(int fd) 
{
  cMutexLock ml(&m_Lock);

  int i;

  for(i=0; i<MAX_UDP_HANDLES; i++)
    if(m_Handles[i] < 0 || m_Handles[i] == fd) {
      m_Handles[i] = fd;
      return true;
    }

  return false;
}

void cUdpScheduler::RemoveHandle(int fd) 
{
  cMutexLock ml(&m_Lock);

  int i;
  for(i=0; i<MAX_UDP_HANDLES; i++)
    if(m_Handles[i] == fd)
      break;

  for(; i<MAX_UDP_HANDLES-1; i++)
    m_Handles[i] = m_Handles[i+1];

  m_Handles[MAX_UDP_HANDLES-1] = -1;

  if(m_Handles[0] < 0) {
    // No clients left ...

    // Flush all buffers
    m_QueueNextSeq = 0;
    m_QueuePending = 0;
    delete m_BackLog; 
    m_BackLog = new cUdpBackLog;
  }
}

bool cUdpScheduler::Poll(int TimeoutMs, bool Master)
{
  cMutexLock ml(&m_Lock);

  m_Master = Master;

  if(m_Handles[0] < 0) {
    // no clients, so we can eat all data we are given ...
    return true;
  }
  
  uint64_t WaitEnd = cTimeMs::Now();
  if(TimeoutMs >= 0)
    WaitEnd += (uint64_t)TimeoutMs;

  int limit = m_Master ? MAX_QUEUE_SIZE : MAX_LIVE_QUEUE_SIZE;
  while(cTimeMs::Now() < WaitEnd &&
	m_Running &&
	m_QueuePending >= limit)
    m_Cond.TimedWait(m_Lock, 5);

  return m_QueuePending < limit;
}

bool cUdpScheduler::Flush(int TimeoutMs)
{
  uint64_t WaitEnd = cTimeMs::Now();
  if(TimeoutMs >= 0)
    WaitEnd += (uint64_t)TimeoutMs;

  cMutexLock ml(&m_Lock);

  if(m_Handles[0] < 0)
    return true;
  
  while(cTimeMs::Now() < WaitEnd &&
	m_Running &&
	m_QueuePending > 0)
    m_Cond.TimedWait(m_Lock, 25);

  return m_QueuePending == 0;
}

void cUdpScheduler::Clear(void)
{
  cMutexLock ml(&m_Lock);

  m_BackLog->Clear(m_QueuePending);

  m_QueuePending = 0;
  m_Cond.Broadcast();
}

bool cUdpScheduler::Queue(uint64_t StreamPos, const uchar *Data, int Length) 
{
  cMutexLock ml(&m_Lock);

  if(m_Handles[0] < 0) 
    return true;

  if(m_QueuePending >= MAX_QUEUE_SIZE)
    return false;

  m_BackLog->MakeFrame(StreamPos, Data, Length); 
  m_QueuePending++;

  m_Cond.Broadcast();

  return true;
}

int cUdpScheduler::calc_elapsed_vtime(int64_t pts, bool Audio) 
{
  int64_t diff = 0;

  if(!Audio /*Video*/) {
    /* #warning TODO: should be possible to use video pts too (if ac3 or muted ...) */
    diff = pts - current_video_vtime;
    if(diff < 0) diff = -diff;
    if(diff > JUMP_LIMIT_TIME) { // 1 s (must be > GOP)
      // RESET
#ifdef LOG_SCR
      LOGDBG("cUdpScheduler RESET (Video jump %lld->%lld)",
	     current_audio_vtime, pts);
      data_sent = frames_sent = 0;
#endif
      //diff = 0;
      current_video_vtime = pts;
      return -1;
    }
    current_video_vtime = pts;
    
  } else if(Audio) {
    diff = pts - current_audio_vtime;
    if(diff < 0) diff = -diff;
    if(diff > JUMP_LIMIT_TIME) { // 1 sec
      // RESET
#ifdef LOG_SCR
      LOGDBG("cUdpScheduler RESET (Audio jump %lld->%lld)",
	     current_audio_vtime, pts);
      data_sent = frames_sent = 0;
#endif
      //diff = 0;
      current_audio_vtime = pts;

      // Use audio pts for sync (audio has constant and increasing intervals)
      MasterClock.Set(current_audio_vtime + INITIAL_BURST_TIME);
      
      return -1;
    }
    current_audio_vtime = pts;
  }
 
#ifdef LOG_SCR
  if(diff && Audio) {
    frame_rate = (int)(90000*frames_sent/(int)diff);
    LOGDBG("rate %d kbit/s (%d frames/s)",
	   (int)(90*data_sent/((int)diff)*8), frame_rate);
    prev_frames = frames_sent;
    data_sent = frames_sent = 0;
  }
#endif
  
  return (int) diff;
}

void cUdpScheduler::Send_RTCP(int fd_rtcp, uint32_t Frames, uint64_t Octets)
{
  uint64_t scr = RtpScr.Now();

  if(scr > (m_LastRtcpTime + RTCP_MIN_INTERVAL)) {
    uint8_t frame[2048], *content = frame;
    rtcp_packet_t  *msg = (rtcp_packet_t *)content;
    struct timeval tv;
    gettimeofday(&tv,NULL);

    // SR (Sender report)
    msg->hdr.raw[0] = 0x81;     // RTP version = 2, Report count = 1 */
    msg->hdr.ptype  = RTCP_SR;
    msg->hdr.length = htons(6); // length 6 dwords

    msg->sr.ssrc     = htonl(m_ssrc);
    msg->sr.ntp_sec  = htonl(tv.tv_sec + 0x83AA7E80);
    msg->sr.ntp_frac = htonl((uint32_t)((double)tv.tv_usec*(double)(1LL<<32)*1.0e-6));
    msg->sr.rtp_ts   = htonl((uint32_t)(scr    & 0xffffffff));
    msg->sr.psent    = htonl((uint32_t)(Frames & 0xffffffff));
    msg->sr.osent    = htonl((uint32_t)(Octets & 0xffffffff));

    content += sizeof(rtcp_common_t) + sizeof(rtcp_sr_t);
    msg = (rtcp_packet_t *)content;

    // SDES
    msg->hdr.raw[0] = 0x81;       // RTP version = 2, Report count = 1 */
    msg->hdr.ptype  = RTCP_SDES;  
    msg->hdr.count  = 1;

    msg->sdes.ssrc   = m_ssrc;
    msg->sdes.item[0].type   = RTCP_SDES_CNAME;
    sprintf(msg->sdes.item[0].data, "VDR@%s:%d%c%c%c", xc.remote_rtp_addr, xc.remote_rtp_port, 0, 0, 0);
    msg->sdes.item[0].length = strlen(msg->sdes.item[0].data);

    msg->hdr.length = htons(1 + 1 + ((msg->sdes.item[0].length - 2) + 3) / 4); 
    
    content += sizeof(rtcp_common_t) + 4*ntohs(msg->hdr.length);
    msg = (rtcp_packet_t *)content;

    // Send
    int err = send(fd_rtcp, frame, content - frame, 0);
#ifdef LOG_RTCP
    LOGMSG("RTCP send (%d)", err);
    for(int i=0; i<content-frame; i+=16) 
      LOGMSG("%02X %02X %02X %02X %02X %02X %02X %02X  "
	     "%02X %02X %02X %02X %02X %02X %02X %02X  "
	     "  %c%c%c%c%c%c%c%c %c%c%c%c%c%c%c%c",
	     frame[i+0],frame[i+1],frame[i+2],frame[i+3],
	     frame[i+4],frame[i+5],frame[i+6],frame[i+7],
	     frame[i+8],frame[i+9],frame[i+10],frame[i+11],
	     frame[i+12],frame[i+13],frame[i+14],frame[i+15],
	     frame[i+0],frame[i+1],frame[i+2],frame[i+3],
	     frame[i+4],frame[i+5],frame[i+6],frame[i+7],
	     frame[i+8],frame[i+9],frame[i+10],frame[i+11],
	     frame[i+12],frame[i+13],frame[i+14],frame[i+15]);
#endif
  }
}

void cUdpScheduler::Schedule(const uchar *Data, int Length)
{
  bool Audio=false, Video=false;
  int64_t pts = pes_extract_pts(Data, Length, Audio, Video);
  int elapsed = pts>0 ? calc_elapsed_vtime(pts, Audio) : 0;

#ifdef LOG_SCR
  if(elapsed > 0)
    LOGMSG("PTS: %lld  (%s) elapsed %d ms (PID %02x)", 
	   pts, Video?"Video":Audio?"Audio":"?", elapsed/90, Data[3]);
#endif

  if(elapsed > 0 && Audio/*Video*/) {
    int64_t now = MasterClock.Now();
    if(now > current_audio_vtime && (now - current_audio_vtime)>JUMP_LIMIT_TIME) {
#ifdef LOG_SCR
      LOGMSG("cUdpScheduler MasterClock init (was in past)");
      elapsed = -1;
#endif
      MasterClock.Set(current_audio_vtime + INITIAL_BURST_TIME);
    } else if(now < current_audio_vtime && (current_audio_vtime-now)>JUMP_LIMIT_TIME) {
#ifdef LOG_SCR
      LOGMSG("cUdpScheduler MasterClock init (was in future)");
      elapsed = -1;
#endif
      MasterClock.Set(current_audio_vtime + INITIAL_BURST_TIME);
    } else if(!last_delay_time) {
      // first burst done, no delay yet ???
      // (queue up to xxx bytes first)
    } else {
      if(current_audio_vtime > now) {
	int delay_ms = (int)(current_audio_vtime - now)/90;
#ifdef LOG_SCR
	LOGDBG("cUdpScheduler sleeping %d ms "
	       "(time reference: %s, beat interval %d ms)",
	       delay_ms, (Audio?"Audio PTS":"Video PTS"), elapsed);
#endif
	if(delay_ms > 3) {
	  //LOGMSG("sleep %d ms (%d f)", delay_ms, prev_frames);
	  CondWait.Wait(delay_ms);
	}
      }
    }
    last_delay_time = now;
  }
  
#if 0
  static int win = 0;
  static int64_t prev;

  if(data_sent == 0 || elapsed < 0) {
    win = 0;
    prev = MasterClock.Now();
  }
  win ++;
  int mrate = 3*frame_rate/2;
  if(mrate < 100) mrate = 100;
  if(mrate > 2000) mrate = 2000;
  if(MasterClock.Now() - prev >= win*90000 / frame_rate) {
    LOGMSG("sleep:3");
    CondWait.Wait(3);
  }
#endif

#ifdef LOG_SCR
  data_sent += Length;
  frames_sent ++;
#endif
}

void cUdpScheduler::Action(void)
{
#if 0
  {
    // Request real-time scheduling
    sched_param temp;
    temp.sched_priority = 2;
    
    if (!pthread_setschedparam(pthread_self(), SCHED_RR, &temp)) {
      LOGMSG("cUdpScheduler priority set successful SCHED_RR %d [%d,%d]",
	     temp.sched_priority,
	     sched_get_priority_min(SCHED_RR),
	     sched_get_priority_max(SCHED_RR));
    } else {
      LOGMSG("cUdpScheduer: Can't set priority to SCHED_RR %d [%d,%d]",
	     temp.sched_priority,
	     sched_get_priority_min(SCHED_RR),
	     sched_get_priority_max(SCHED_RR));
    }

    /* UDP Scheduler needs high priority */
    SetPriority(0);  
    SetPriority(-1); 
    SetPriority(-2);
    SetPriority(-3);
    SetPriority(-4);
    SetPriority(-5);
  }
#endif

  m_Lock.Lock();

  while(m_Running) {
	
    if(m_Handles[0] < 0) {
      m_Cond.TimedWait(m_Lock, 5000); 
      continue;
    }

    // Wait until we have outgoing data in queue
    if(m_QueuePending <= 0) {
      m_Cond.TimedWait(m_Lock, 100); 
      if(m_QueuePending <= 0) {
	static unsigned char padding[] = {0x00,0x00,0x01,0xBE,0x00,0x02,0xff,0xff};
	int prevseq = (m_QueueNextSeq + UDP_BUFFER_SIZE - 1) & UDP_BUFFER_MASK;
	stream_udp_header_t *frame = m_BackLog->Get(prevseq);
	if(frame) {
	  int prevlen = m_BackLog->PayloadSize(prevseq);
	  uint64_t pos = ntohll(frame->pos) + prevlen - 8;
	  m_BackLog->MakeFrame(pos, padding, 8);
	} else
	  m_BackLog->MakeFrame(0, padding, 8);
	m_QueuePending++;
      }
      continue; // to check m_Running
    }

    // Take next frame from queue
    stream_udp_header_t *frame = m_BackLog->Get(m_QueueNextSeq);
    int PayloadSize = m_BackLog->PayloadSize(m_QueueNextSeq);
    int UdpPacketLen = PayloadSize + sizeof(stream_udp_header_t);
    m_QueueNextSeq = (m_QueueNextSeq + 1) & UDP_BUFFER_MASK;
    m_QueuePending--;

    m_Cond.Broadcast();
 
    m_Lock.Unlock();

#if 0 /* debugging checks */
    {
      if(!frame)
	LOGMSG("frame == NULL !");
      uint8_t *p = UDP_PAYLOAD(frame);

      if(p[0] || p[1] || p[2]!=1)
	LOGMSG("cUdpScheduler: invalid content");

      int n = sizeof(stream_udp_header_t) + (p[4]<<8) + p[5] + 6;
      if(n != UdpPacketLen)
	LOGMSG("cUdpScheduler: length error -- %d != %d", n, UdpPacketLen);

      static int seq = 0;
      if(seq != ntohs(frame->seq))
	LOGMSG("cUdpScheduler: SEQ jump %d -> %d !", seq, ntohs(frame->seq));
      seq = (ntohs(frame->seq) + 1) & UDP_BUFFER_MASK;

      if(PayloadSize != 8) {
	static uint64_t pos = 0;
	if(pos != ntohull(frame->pos))
	  LOGMSG("cUdpScheduler: POS jump %lld -> %lld !", pos, ntohull(frame->pos));
	pos = ntohull(frame->pos) + PayloadSize;
      }
    }
#endif

    // Schedule frame
    if(m_Master)
      Schedule(UDP_PAYLOAD(frame), PayloadSize);

    /* need some limit here for ex. sequence of stills when moving cutting marks very fast
       (no audio or PTS available) */
#if 1
    // hard limit for used bandwidth:
    // - ~1 frames/ms & 8kb/ms -> 8mb/s -> ~ 80 Mbit/s ( / client)
    // - max burst 15 frames or 30kb
    static int cnt = 0, bytes = 0;
    static uint64_t dbg_timer = cTimeMs::Now();
    static int dbg_bytes = 0;
    cnt++; 
    bytes += PayloadSize;
    if(cnt>=15 && bytes >= 30000) {
      CondWait.Wait(4);
      dbg_bytes += bytes;
      cnt = 0; 
      bytes = 0;
      if(dbg_timer+60000 <= cTimeMs::Now()) {
	LOGDBG("UDP rate: %4d Kbps (queue %d)", dbg_bytes/(60*1024/8),
	       m_QueuePending);
	dbg_bytes = 0;
	dbg_timer = cTimeMs::Now();
      }
    }
#endif

    for(int i=0; i<MAX_UDP_HANDLES && m_Handles[i]>=0; i++) {

      //
      // use TIOCOUTQ ioctl instead of poll/select.
      // - poll/select for UDP/RTP may return true even when queue 
      //   is (almost) full
      // - kernel silently drops frames it cant send
      // -> poll() + send() just causes frames to be dropped
      //
      int size = 0;
      if(!ioctl(m_Handles[i], TIOCOUTQ, &size))
	if(size > ((0x10000)/2 - 2048)) { // assume 64k kernel buffer
	  int wmem=0;
	  socklen_t l = sizeof(int);
	  if(!getsockopt(m_Handles[i], SOL_SOCKET, SO_SNDBUF, &wmem, &l)) {
#if 0
// Large bursts cause client to loose data :(
	    if(size >= (wmem/2 - 8128)) {
	      LOGMSG("cUdpScheduler: kernel transmit queue > ~%dkb ! (master=%d)", 
		     (wmem/2-8128)/1024, m_Master);
	      CondWait.Wait(2);
	    } 
	    else 
#endif
	    {
	      if(m_QueuePending > (MAX_QUEUE_SIZE-5))
		LOGDBG("cUdpScheduler: kernel transmit queue > ~30kb ! (master=%d ; Queue=%d)", 
		       m_Master, m_QueuePending);
	      CondWait.Wait(2);
	    }
	  }
	}
      
      if(send(m_Handles[i], frame, UdpPacketLen, 0) <= 0)
	LOGERR("cUdpScheduler: UDP send() failed !");
    }

    m_Lock.Lock();
  }
  
  m_Lock.Unlock();
}

void cUdpScheduler::ReSend(int fd, uint64_t Pos, int Seq1, int Seq2) 
{
  cMutexLock ml(&m_Lock); // keeps also scheduler thread suspended ...

  // Handle buffer wrap
  if(Seq1 > Seq2)
    Seq2 += UDP_BUFFER_SIZE;

  if(Seq2-Seq1 > 64) {
    LOGDBG("cUdpScheduler::ReSend: requested range too large (%d-%d)",
	   Seq1, Seq2);
    return;
  }

  // re-send whole range
  for(; Seq1 <= Seq2; Seq1++) {
      
    // Wait if kernel queue is full
    int size=0;
    if(!ioctl(fd, TIOCOUTQ, &size))
      if(size > ((0x10000)/2 - 2048)) { // assume 64k kernel buffer
	LOGDBG("cUdpScheduler::ReSend: kernel transmit queue > ~30kb !");
	cCondWait::SleepMs(2);
      }
    
    stream_udp_header_t *frame = m_BackLog->Get(Seq1);
      
    if(frame) {
      if(ntohull(frame->pos) == Pos) {
	send(fd, 
	     frame, 
	     m_BackLog->PayloadSize(Seq1) + sizeof(stream_udp_header_t), 
	     0);
#ifdef LOG_RESEND
	LOGDBG("cUdpScheduler::ReSend: %d (%d bytes) @%lld sent", 
	       Seq1, m_BackLog->PayloadSize(Seq1), Pos);
#endif
	Pos += m_BackLog->PayloadSize(Seq1);
	continue;
      } else {
	// buffer has been lost long time ago...
#ifdef LOG_RESEND
	LOGDBG("cUdpScheduler::ReSend: Requested position does not match "
	       "(%lld ; has %lld)", Pos, ntohll(frame->pos));
#endif
      }
    } else {
#ifdef LOG_RESEND
      LOGDBG("cUdpScheduler::ReSend: %d @%lld missing", Seq1, Pos);
#endif
    }
    // buffer has been lost
    // send packet missing info
    char udp_ctrl[64];
    ((stream_udp_header_t *)udp_ctrl)->seq = (uint16_t)(-1);
    ((stream_udp_header_t *)udp_ctrl)->pos = (uint64_t)(-1);
    
#ifdef LOG_RESEND
    LOGDBG("cUdpScheduler::ReSend: missing %d-%d @%d (hdr 0x%llx 0x%x)",
	   Seq1, Seq1, Pos,
	   ((stream_udp_header_t *)udp_ctrl)->pos,
	   ((stream_udp_header_t *)udp_ctrl)->seq);
#endif
    sprintf((udp_ctrl+sizeof(stream_udp_header_t)),
	    "UDP MISSING %d-%d %" PRIu64,
	    Seq1, Seq1, Pos);
    
    send(fd, udp_ctrl, 64, 0);
  }
}
