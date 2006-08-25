/*
 * xine_fbfe_frontend.c: Simple front-end, framebuffer functions
 *
 * See the main source file 'xineliboutput.c' for copyright information and
 * how to reach the author.
 *
 * $Id: xine_fbfe_frontend.c,v 1.7 2006-08-25 03:55:05 phintuka Exp $
 *
 */

#include <errno.h>
#include <inttypes.h>
#include <poll.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dlfcn.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>
#include <sched.h>
#include <linux/unistd.h> /* gettid() */

#ifdef boolean
# define HAVE_BOOLEAN
#endif
#include <jpeglib.h>
#undef boolean

#include <xine.h>
#ifndef XINE_ENGINE_INTERNAL
#  define XINE_ENGINE_INTERNAL
#  include <xine/xine_internal.h>
#  undef XINE_ENGINE_INTERNAL
#else
#  include <xine/xine_internal.h>
#endif
#include <xine/xineutils.h>
#include <xine/input_plugin.h>
#include <xine/plugin_catalog.h>

#include "xine_input_vdr.h"

#include "xine_frontend.h"
#include "xine/post.h"

/*
 * data
 */

typedef struct fbfe_t {

  /* function pointers */
  frontend_t         fe;
  void (*update_display_size)(frontend_t*);

  /* xine stuff */
  xine_t             *xine;
  xine_stream_t      *stream;
  input_plugin_t     *input;
  xine_video_port_t  *video_port;
  xine_audio_port_t  *audio_port;
  xine_event_queue_t *event_queue;

  post_plugins_t     *postplugins;

  char                configfile[256];

  int                 xine_visual_type;
  fb_visual_t         vis;

  double              display_ratio;
  int                 overscan;

  int                 pes_buffers;
  int                 aspect;
  int                 cropping;
  int                 scale_video;
  int                 priority;

  /* frontend */
  int                 playback_finished;
  int                 slave_playback_finished;

  /* vdr */
  fe_keypress_f       keypress;

  /* display */
  int                 display;
  int                 fullscreen;
  int                 vmode_switch;
  int                 field_order;
  char                modeline[256];

  int                 xpos, ypos;
  int                 width, height;

  int                 video_width, video_height;

} fe_t;

/* Common (non-X11/FB) frontend functions */
#include "xine_frontend.c"

static void fbfe_update_display_size(frontend_t *this_gen)
{
  fe_t *this = (fe_t*)this_gen;
  if(this->fullscreen) {
    this->width  = this->video_port->get_property(this->video_port, 
						  VO_PROP_WINDOW_WIDTH);
    this->height = this->video_port->get_property(this->video_port, 
						  VO_PROP_WINDOW_HEIGHT);
    LOGDBG("Framebuffer size after initialization: %dx%d",
	   this->width, this->height);
  }
}

/*
 * fbfe_display_open
 */
static int fbfe_display_open(frontend_t *this_gen, int width, int height, int fullscreen,
			     int modeswitch, const char *modeline, int aspect,
			     fe_keypress_f keyfunc, const char *video_port,
			     int scale_video, int field_order) 
{
  fe_t *this = (fe_t*)this_gen;

  if(!this)
    return 0;

  if(this->display)
    this->fe.fe_display_close(this_gen);

  this->display = 1;
  if(keyfunc)
    this->keypress = keyfunc;

  LOGDBG("fbfe_display_open(width=%d, height=%d, fullscreen=%d, display=%s)",
	 width, height, fullscreen, video_port);

  this->xpos            = 0;
  this->ypos            = 0;
  this->width           = width;
  this->height          = height;
  this->fullscreen      = fullscreen;
  this->vmode_switch    = 0/*modeswitch*/;
  this->aspect          = aspect;
  this->cropping        = 0;
  this->field_order     = 0/*field_order ? 1 : 0*/;
  this->scale_video     = scale_video;
  this->overscan        = 0;
  strcpy(this->modeline, modeline);
  this->display_ratio   = 1.0;

  this->xine_visual_type = XINE_VISUAL_TYPE_FB;
  this->vis.frame_output_cb = fe_frame_output_cb;
  this->vis.user_data = this;

  this->update_display_size = fbfe_update_display_size;

  return 1;
}

/*
 * fbfe_display_config
 *
 * configure windows
 */
static int fbfe_display_config(frontend_t *this_gen, int width, int height, int fullscreen, 
			       int modeswitch, const char *modeline, int aspect, 
			       int scale_video, int field_order) 
{
  fe_t *this = (fe_t*)this_gen;

  if(!this)
    return 0;

  if(this->width != width || this->height != height) {
    this->width           = width;
    this->height          = height;
  }

  if(fullscreen != this->fullscreen) {
    this->fullscreen = fullscreen;
  }

  if(!modeswitch && strcmp(modeline, this->modeline)) {
    strcpy(this->modeline, modeline);
    /* XXX TODO - switch vmode */
#ifdef LOG
    LOGDBG("fbfe_display_config: TODO: switch vmode\n");fflush(stdout);
#endif
  }

  this->vmode_switch = modeswitch;
  this->aspect = aspect;
  this->scale_video = scale_video;
  this->field_order = field_order ? 1 : 0;

  return 1;
}

static void fbfe_interrupt(frontend_t *this_gen) 
{
  /* stop fbfe_run() */
}

static int fbfe_run(frontend_t *this_gen) 
{
  struct timeval tv;
  fe_t *this = (fe_t*)this_gen;

  if(this && this->playback_finished)
    return !this->playback_finished;

  tv.tv_sec = 0;
  tv.tv_usec = 500*1000;
  select(0, NULL, NULL, NULL, &tv); /* just sleep 500ms */

  return !(!this || this->playback_finished);
}

static void fbfe_display_close(frontend_t *this_gen) 
{
  fe_t *this = (fe_t*)this_gen;

  if(this) {
    this->display = 0;

    if(this->xine)
      this->fe.xine_exit(this_gen);
  }
}

static frontend_t *fbfe_get_frontend(void)
{
  fe_t *this = malloc(sizeof(fe_t));
  memset(this, 0, sizeof(fe_t));

  this->fe.fe_display_open   = fbfe_display_open;
  this->fe.fe_display_config = fbfe_display_config;
  this->fe.fe_display_close  = fbfe_display_close;

  this->fe.xine_init  = fe_xine_init;
  this->fe.xine_open  = fe_xine_open;
  this->fe.xine_play  = fe_xine_play;
  this->fe.xine_stop  = fe_xine_stop;
  this->fe.xine_close = fe_xine_close;
  this->fe.xine_exit  = fe_xine_exit;
  this->fe.xine_is_finished = fe_is_finished;

  this->fe.fe_run       = fbfe_run;
  this->fe.fe_interrupt = fbfe_interrupt;

  this->fe.fe_free      = fe_free;

  this->fe.grab                  = fe_grab;
#ifndef FE_STANDALONE
  this->fe.xine_osd_command      = xine_osd_command;
  this->fe.xine_control          = xine_control;

  this->fe.xine_queue_pes_packet = xine_queue_pes_packet;
#endif /*#ifndef FE_STANDALONE */

  return (frontend_t*)this;
}

/* ENTRY POINT */
const fe_creator_f fe_creator __attribute__((visibility("default"))) = fbfe_get_frontend;


