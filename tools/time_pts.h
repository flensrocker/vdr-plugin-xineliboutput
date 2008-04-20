/*
 * time_pts.h: Adjustable clock in PTS units
 *
 * See the main source file 'xineliboutput.c' for copyright information and
 * how to reach the author.
 *
 * $Id: time_pts.h,v 1.3 2008-04-20 21:29:29 phintuka Exp $
 *
 */

#ifndef __TIME_PTS_H
#define __TIME_PTS_H

#include <stdint.h>   // int64_t
#include <sys/time.h> // struct timeval


class cTimePts 
{
  private:
    int64_t begin;          /* Start time (PTS) */
    struct timeval tbegin;  /* Start time (real time) */
    bool m_Paused;
    int  m_Multiplier;
    int  m_ScrSpeed;

    static int m_Monotonic;
    static void Init(void);

  public:
    cTimePts(void);

    int64_t Now(void);
    void    Set(int64_t Pts = 0LL);

    void Pause(void);
    void Resume(void);
    void TrickSpeed(int Multiplier);

    void SetScrSpeed(int ScrSpeed = 90000);
};

#endif // __TIME_PTS_H
