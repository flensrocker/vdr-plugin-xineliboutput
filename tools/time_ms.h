/*
 * time_ms.h: simple current time interface
 *
 * See the main source file 'xineliboutput.c' for copyright information and
 * how to reach the author.
 *
 * $Id: time_ms.h,v 1.1 2012-02-17 09:15:42 phintuka Exp $
 *
 */

#ifndef XINELIBOUTPUT_TIME_MS_H_
#define XINELIBOUTPUT_TIME_MS_H_

#include <sys/time.h>

static uint64_t time_ms(void)
{
  struct timeval t;
  if (gettimeofday(&t, NULL) == 0)
     return ((uint64_t)t.tv_sec) * 1000ULL + t.tv_usec / 1000ULL;
  return 0;
}

static uint64_t elapsed(uint64_t t)
{
  return time_ms() - t;
}

#endif /* XINELIBOUTPUT_TIME_MS_H_ */
