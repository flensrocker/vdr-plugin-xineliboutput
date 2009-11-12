/*
 * xvdr_metronom.c:
 *
 * See the main source file 'xineliboutput.c' for copyright information and
 * how to reach the author.
 *
 * $Id: xvdr_metronom.c,v 1.1 2009-05-22 21:02:30 phintuka Exp $
 *
 */

#include <stdlib.h>

#include <xine/xine_internal.h>
#include <xine/metronom.h>

#define LOG_MODULENAME "[metronom ] "
#define SysLogLevel    iSysLogLevel
#include "../logdefs.h"

#define XVDR_METRONOM_COMPILE
#include "xvdr_metronom.h"


static void got_video_frame(metronom_t *metronom, vo_frame_t *frame)
{
  xvdr_metronom_t *this = (xvdr_metronom_t *)metronom;

  this->video_frames++;

 if (this->frame_decoded)
   this->frame_decoded(this->handle, this->video_frames, this->audio_frames);

  return this->orig_metronom->got_video_frame (this->orig_metronom, frame);
}

static int64_t got_audio_samples(metronom_t *metronom, int64_t pts, int nsamples)
{
  xvdr_metronom_t *this = (xvdr_metronom_t *)metronom;

  this->audio_frames++;

 if (this->frame_decoded)
   this->frame_decoded(this->handle, this->video_frames, this->audio_frames);

  return this->orig_metronom->got_audio_samples (this->orig_metronom, pts, nsamples);
}

/*
 * dummy hooks
 */

static int64_t got_spu_packet(metronom_t *metronom, int64_t pts)
{
  xvdr_metronom_t *this = (xvdr_metronom_t *)metronom;
  return this->orig_metronom->got_spu_packet(this->orig_metronom, pts);
}

static void handle_audio_discontinuity(metronom_t *metronom, int type, int64_t disc_off)
{
  xvdr_metronom_t *this = (xvdr_metronom_t *)metronom;
  this->orig_metronom->handle_audio_discontinuity(this->orig_metronom, type, disc_off);
}

static void handle_video_discontinuity(metronom_t *metronom, int type, int64_t disc_off)
{
  xvdr_metronom_t *this = (xvdr_metronom_t *)metronom;
  this->orig_metronom->handle_video_discontinuity(this->orig_metronom, type, disc_off);
}

static void set_audio_rate(metronom_t *metronom, int64_t pts_per_smpls)
{
  xvdr_metronom_t *this = (xvdr_metronom_t *)metronom;
  this->orig_metronom->set_audio_rate(this->orig_metronom, pts_per_smpls);
}

static void set_option(metronom_t *metronom, int option, int64_t value)
{
  xvdr_metronom_t *this = (xvdr_metronom_t *)metronom;
  this->orig_metronom->set_option(this->orig_metronom, option, value);
}

static int64_t get_option(metronom_t *metronom, int option)
{
  xvdr_metronom_t *this = (xvdr_metronom_t *)metronom;
  return this->orig_metronom->get_option(this->orig_metronom, option);
}

static void set_master(metronom_t *metronom, metronom_t *master)
{
  xvdr_metronom_t *this = (xvdr_metronom_t *)metronom;
  this->orig_metronom->set_master(this->orig_metronom, master);
}

static void metronom_exit(metronom_t *metronom)
{
  xvdr_metronom_t *this = (xvdr_metronom_t *)metronom;

  LOGERR("xvdr_metronom: metronom_exit() called !");

  /* un-hook */
  this->stream->metronom = this->orig_metronom;
  this->orig_metronom = NULL;
  this->stream = NULL;

  this->orig_metronom->exit(this->orig_metronom);
}

/*
 * xvdr_metronom_t
 */

static void xvdr_metronom_set_cb(xvdr_metronom_t *this,
                                 void (*cb)(void*, uint, uint),
                                 void *handle)
{
  this->handle = handle;
  this->frame_decoded = cb;
}

static void xvdr_metronom_dispose(xvdr_metronom_t *this)
{
  if (this->stream && this->orig_metronom)
    this->stream->metronom = this->orig_metronom;

  free(this);
}

static void xvdr_metronom_reset_frames(xvdr_metronom_t *this)
{
  this->video_frames = this->audio_frames = 0;
}

/*
 * init
 */

xvdr_metronom_t *xvdr_metronom_init(xine_stream_t *stream)
{
  xvdr_metronom_t *this = calloc(1, sizeof(xvdr_metronom_t));

  this->stream        = stream;
  this->orig_metronom = stream->metronom;

  this->set_cb       = xvdr_metronom_set_cb;
  this->reset_frames = xvdr_metronom_reset_frames;
  this->dispose      = xvdr_metronom_dispose;

  this->metronom.set_audio_rate    = set_audio_rate;
  this->metronom.got_video_frame   = got_video_frame;
  this->metronom.got_audio_samples = got_audio_samples;
  this->metronom.got_spu_packet    = got_spu_packet;
  this->metronom.handle_audio_discontinuity = handle_audio_discontinuity;
  this->metronom.handle_video_discontinuity = handle_video_discontinuity;
  this->metronom.set_option = set_option;
  this->metronom.get_option = get_option;
  this->metronom.set_master = set_master;

  this->metronom.exit = metronom_exit;

  /* hook up to stream */
  this->stream->metronom = &this->metronom;

  return this;
}