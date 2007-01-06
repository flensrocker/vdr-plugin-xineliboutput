/*
 * time_pts.h: Adjustable clock in PTS units
 *
 * See the main source file 'xineliboutput.c' for copyright information and
 * how to reach the author.
 *
 * $Id: time_pts.h,v 1.1 2007-01-06 03:39:15 phintuka Exp $
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
    bool m_Monotonic;

  public:
    cTimePts(void);

    int64_t Now(void);
    void    Set(int64_t Pts = 0LL);

    void Pause(void);
    void Resume(void);
    void TrickSpeed(int Multiplier);
};

#endif // __TIME_PTS_H
