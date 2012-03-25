/*
 * xvdr_metronom.h:
 *
 * See the main source file 'xineliboutput.c' for copyright information and
 * how to reach the author.
 *
 * $Id: xvdr_metronom.h,v 1.13 2012-03-25 18:41:53 phintuka Exp $
 *
 */

#ifndef XVDR_METRONOM_H
#define XVDR_METRONOM_H

#include <xine/metronom.h>

#ifdef METRONOM_INTERNAL
#  error METRONOM_INTERNAL defined, struct xvdr_metronom_s size will be incorrect
#endif

#define XVDR_METRONOM_OPTION_BASE  0x1001
#define XVDR_METRONOM_LAST_VO_PTS  (XVDR_METRONOM_OPTION_BASE)
#define XVDR_METRONOM_TRICK_SPEED  (XVDR_METRONOM_OPTION_BASE + 1)
#define XVDR_METRONOM_STILL_MODE   (XVDR_METRONOM_OPTION_BASE + 2)
#define XVDR_METRONOM_ID           (XVDR_METRONOM_OPTION_BASE + 3)


typedef struct xvdr_metronom_s xvdr_metronom_t;

struct xvdr_metronom_s {
  /* xine-lib metronom interface */
  metronom_t     metronom;

  /* management interface */
  void (*dispose)     (xvdr_metronom_t *);

  void (*wire)          (xvdr_metronom_t *);
  void (*unwire)        (xvdr_metronom_t *);

  /* private data */

#ifdef XVDR_METRONOM_COMPILE

  /* original metronom */
  metronom_t    *orig_metronom;
  xine_stream_t *stream;

  int     trickspeed;    /* current trick speed */
  int     still_mode;
  int64_t last_vo_pts;   /* last displayed video frame PTS */
  int     wired;         /* true if currently wired to stream */

  pthread_mutex_t mutex;
#endif
};

xvdr_metronom_t *xvdr_metronom_init(xine_stream_t *);


#endif /* XVDR_METRONOM_H */
