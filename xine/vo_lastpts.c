/*
 * vo_lastpts.c:
 *
 * See the main source file 'xineliboutput.c' for copyright information and
 * how to reach the author.
 *
 * $Id: vo_lastpts.c,v 1.1 2010-01-26 11:36:21 phintuka Exp $
 *
 */

#include <stdlib.h>

#include <xine/video_out.h>

#include "vo_hook.h"

/*
 *  lastpts_hook_t
 */
typedef struct {
  vo_driver_hook_t h;

  /* PTS of last displayed video frame */
  int64_t last_pts;

} lastpts_hook_t;


/* next symbol is dynamically linked from input plugin */
int64_t vo_last_video_pts __attribute__((visibility("default"))) = INT64_C(-1);


/*
 * interface
 */

/*
 * override display_frame()
 */

static void lastpts_display_frame(vo_driver_t *self, vo_frame_t *vo_img)
{
  lastpts_hook_t *this = (lastpts_hook_t*)self;

  if (vo_img->pts > 0)
    vo_last_video_pts = vo_img->pts;

  this->h.orig_driver->display_frame(this->h.orig_driver, vo_img);
}


/*
 * init()
 */
vo_driver_t *vo_lastpts_init(void)
{
  lastpts_hook_t *this = calloc(1, sizeof(lastpts_hook_t));

  this->h.vo.display_frame = lastpts_display_frame;

  return &this->h.vo;
}

