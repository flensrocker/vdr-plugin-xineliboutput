/*
 * vo_lastpts.c:
 *
 * See the main source file 'xineliboutput.c' for copyright information and
 * how to reach the author.
 *
 * $Id: vo_lastpts.c,v 1.4 2011-10-25 07:19:29 phintuka Exp $
 *
 */

#include <stdlib.h>

#include <xine/xine_internal.h>
#include <xine/video_out.h>
#include <xine/metronom.h>

#include "xvdr_metronom.h"

#include "vo_hook.h"

#define LOG_MODULENAME "[lastpts  ] "
#include "../logdefs.h"


/*
 *  lastpts_hook_t
 */
typedef struct {
  vo_driver_hook_t  h;
  xine_stream_t    *prev_stream;
  xine_stream_t    *xvdr_stream;
  metronom_t       *xvdr_metronom;
} lastpts_hook_t;

/*
 * interface
 */

/*
 * override display_frame()
 */

static void lastpts_display_frame(vo_driver_t *self, vo_frame_t *vo_img)
{
  lastpts_hook_t *this = (lastpts_hook_t*)self;

  ASSERT_RET(self,   return);
  ASSERT_RET(vo_img, return);

  /* detect intercepted metronom with XVDR_* option support.
   * This prevents flooding log with "unknown option in set_option" messages
   */
  if (vo_img->stream != this->prev_stream && vo_img->stream && vo_img->stream != XINE_ANON_STREAM) {
    LOGMSG("stream changed from %p to %p", this->prev_stream, vo_img->stream);
    this->prev_stream = vo_img->stream;

    if (vo_img->stream->metronom->get_option(vo_img->stream->metronom, XVDR_METRONOM_ID) == XVDR_METRONOM_ID) {
      LOGMSG("new stream is vdr stream");
      this->xvdr_metronom = vo_img->stream->metronom;
      this->xvdr_stream   = vo_img->stream;
    }
  }

  if (this->xvdr_metronom) {

    if (vo_img->stream == this->xvdr_stream) {
      LOGVERBOSE("last pts %"PRId64, vo_img->pts);
      ASSERT_RET(this->xvdr_metronom->set_option, return);

      this->xvdr_metronom->set_option(this->xvdr_metronom, XVDR_METRONOM_LAST_VO_PTS, vo_img->pts);

    } else {
      LOGVERBOSE("last pts %"PRId64" - %p not vdr stream", vo_img->pts, vo_img->stream);
    }
  }

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

