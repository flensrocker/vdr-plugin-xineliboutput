/*
 * udp_pes_scheduler.h: PES scheduler for UDP/RTP streams
 *
 * See the main source file 'xineliboutput.c' for copyright information and
 * how to reach the author.
 *
 * $Id: udp_pes_scheduler.h,v 1.5 2006-10-28 02:30:11 phintuka Exp $
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
    int64_t begin;          /* Start time (PTS) */
    struct timeval tbegin;  /* Start time (real time) */
    bool m_Paused;
    int  m_Multiplier;
    bool m_Monotonic;

  public:
    cTimePts(void);

    int64_t Now(void);
    void    Set(int64_t Pts = 0LL);

    void Pause(void);
    void Resume(void);
    void TrickSpeed(int Multiplier);
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
    bool AddHandle(int fd, int fd_rtcp=-1);
    void RemoveHandle(int fd);

    bool Poll(int TimeoutMs, bool Master);
    bool Queue(uint64_t StreamPos, const uchar *Data, int Length);
    void ReSend(int fd, uint64_t Pos, int Seq1, int Seq2);

    void Clear(void);
    bool Flush(int TimeoutMs);

    void Pause(bool On);
    void TrickSpeed(int Multiplier);

  protected:

    // Data for payload handling & buffering

    // Signalling
    cCondVar  m_Cond;
    cMutex    m_Lock;

    // Clients
    int       m_Handles[MAX_UDP_HANDLES];
    int       m_fd_rtp, m_fd_rtcp;

    // Queue
    int m_QueueNextSeq;      /* next outgoing */
    int m_QueuePending;      /* outgoing queue size */
    cUdpBackLog *m_BackLog;  /* queue for incoming data (not yet send) and retransmissions */

    // Data for scheduling algorithm
    cTimePts  MasterClock;   /* Current MPEG PTS (synchronized to current stream) */
    cCondWait CondWait;

    int64_t  current_audio_vtime;
    int64_t  current_video_vtime;

    // RTP
    uint32_t  m_ssrc;   /* RTP synchronization source id */
    cTimePts  RtpScr;   /* 90 kHz monotonic time source for RTP timestamps */
    uint64_t  m_LastRtcpTime;
    uint32_t  m_Frames;
    uint32_t  m_Octets;

    int64_t last_delay_time;

    bool m_Master;   /* if true, we are master metronom for playback */

    // Scheduling 
  
    int calc_elapsed_vtime(int64_t pts, bool Audio);
    void Schedule(const uchar *Data, int Length);

    bool m_Running;
    virtual void Action(void);

    void Send_RTCP(void);
};

#endif
