/*
 * udp_pes_scheduler.h: PES scheduler for UDP/RTP streams
 *
 * See the main source file 'xineliboutput.c' for copyright information and
 * how to reach the author.
 *
 * $Id: udp_pes_scheduler.h,v 1.1 2006-06-03 10:04:28 phintuka Exp $
 *
 */

#ifndef __UDP_PES_SCHEDULER_H
#define __UDP_PES_SCHEDULER_H

#include <stdint.h>

#include <vdr/tools.h>  // uchar
#include <vdr/thread.h>

//----------------------- cTimePts ------------------------------------------

class cTimePts 
{
  private:
    int64_t begin;
    struct timeval tbegin;

  public:
    cTimePts(void);

    int64_t Now(void);
    void    Set(int64_t Pts = 0LL);
};

//----------------------- cUdpPesScheduler ----------------------------------

#define MAX_UDP_HANDLES 16

class cUdpBackLog;

class cUdpScheduler : public cThread
{
  public:

    cUdpScheduler();
    virtual ~cUdpScheduler();

    // fd should be binded & connected to IP:PORT (local+remote) pair !
    bool AddHandle(int fd);
    void RemoveHandle(int fd);

    bool Poll(int TimeoutMs, bool Master);
    bool Queue(uint64_t StreamPos, const uchar *Data, int Length);
    void ReSend(int fd, uint64_t Pos, int Seq1, int Seq2);

    void Clear(void);
    bool Flush(int TimeoutMs);

  protected:

    // Data for payload handling & buffering

    // Signalling
    cCondVar  m_Cond;
    cMutex    m_Lock;

    // Clients
    int       m_Handles[MAX_UDP_HANDLES];

    // Queue
    int m_QueueNextSeq;      /* next outgoing */
    int m_QueuePending;      /* outgoing queue size */
    cUdpBackLog *m_BackLog;  /* queue for incoming data (not yet send) and retransmissions */

    // Data for scheduling algorithm

    cTimePts  RtpScr;      // 90 kHz monotonic time source for RTP packets
    cTimePts  MasterClock; // Current MPEG PTS (synchronized with current stream)
    cCondWait CondWait;

    int64_t  current_audio_vtime;
    int64_t  current_video_vtime;
    
#if 0
    int data_sent;   /* in current time interval, bytes */
    int frames_sent; /* in current time interval */
    int frame_rate;  /* pes frames / second */
    int prev_frames;
#endif

    int64_t last_delay_time;

    bool m_Master;   /* if true, we are master metronom for playback */

    // Scheduling 
  
    int calc_elapsed_vtime(int64_t pts, bool Audio);
    void Schedule(const uchar *Data, int Length);

    bool m_Running;
    virtual void Action(void);
};

#endif
