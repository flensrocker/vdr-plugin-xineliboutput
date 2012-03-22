/*
 * xvdr_metronom.h:
 *
 * See the main source file 'xineliboutput.c' for copyright information and
 * how to reach the author.
 *
 * $Id: xvdr_metronom.h,v 1.9 2012-03-22 12:18:51 phintuka Exp $
 *
 */

#ifndef XVDR_METRONOM_H
#define XVDR_METRONOM_H


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
  void (*set_cb)      (xvdr_metronom_t *,
                       void (*cb) (void *, uint, uint),
                       void *);
  void (*reset_frames)(xvdr_metronom_t *);
  void (*dispose)     (xvdr_metronom_t *);

  void (*set_trickspeed)(xvdr_metronom_t *, int);
  void (*set_still_mode)(xvdr_metronom_t *, int);

  void (*wire)          (xvdr_metronom_t *);
  void (*unwire)        (xvdr_metronom_t *);

  /* accumulated frame data */
  volatile uint video_frames;
  volatile uint audio_frames;

  /* private data */

#ifdef XVDR_METRONOM_COMPILE

  /* original metronom */
  metronom_t    *orig_metronom;
  xine_stream_t *stream;

  /* callback */
  void *handle;
  void (*frame_decoded)(void *handle, uint video_count, uint audio_count);

  int     trickspeed;    /* current trick speed */
  int     still_mode;
  int64_t last_vo_pts;   /* last displayed video frame PTS */
  int     wired;         /* true if currently wired to stream */

  pthread_mutex_t pts_mutex;
#endif
};

xvdr_metronom_t *xvdr_metronom_init(xine_stream_t *);


#endif /* XVDR_METRONOM_H */
