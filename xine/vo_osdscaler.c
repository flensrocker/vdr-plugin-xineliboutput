/*
 * vo_osdscaler.c: OSD scaling video-out post plugin
 *
 * See the main source file 'xineliboutput.c' for copyright information and
 * how to reach the author.
 *
 * $Id: vo_osdscaler.c,v 1.2 2008-12-18 20:03:13 phintuka Exp $
 *
 */

#include <stdlib.h>

#include <xine/video_out.h>
#ifdef SWBLEND
# include <xine/alphablend.h>
#endif

#define LOG_MODULENAME "[osdscaler] "
#include "../logdefs.h"

#include "vo_props.h"
#include "vo_hook.h"

/*#define LOGOSD(x...) LOGMSG(x)*/
#define LOGOSD(x...)

typedef rle_elem_t xine_rle_elem_t;

#include "../tools/rle.h"


/* Make sure our properties won't overlap with xine's properties */
#if VO_NUM_PROPERTIES > VO_PROP_OSD_SCALING
# error VO_NUM_PROPERTIES > VO_PROP_OSD_SCALING
#endif

#undef  ABS
#define ABS(x) ((x)<0?-(x):(x))


/*
 * osd_data_t
 *
 * - cache scaled OSD data
 */

typedef struct osd_data_s osd_data_t;

struct osd_data_s {
  /* original source */
  vo_overlay_t *source;

  /* scaled overlay */
  uint8_t      scaled : 1;
  vo_overlay_t ovl;

  /* for what output resolution overlay was scaled */
  uint16_t     output_width;
  uint16_t     output_height;

  /* linked list */
  osd_data_t  *next;
};

/*
 * osd_data_dispose()
 *
 *  - free() osd_data_t and all allocated memory
 */
static void osd_data_dispose(osd_data_t *data)
{
  if (data->scaled)
    free(data->ovl.rle);
  free(data);
}

/*
 * osd_data_clear()
 *
 *  - free() whole linked list
 */
static void osd_data_clear(osd_data_t *data)
{
  if (data) {
    if (data->next)
      osd_data_clear(data->next);
    osd_data_dispose(data);
  }
}

/*
 * osd_data_remove()
 *
 *  - remove (and free) specific osd_data_t item from linked list
 */
static void osd_data_remove(osd_data_t **list, osd_data_t *data)
{
  if (!list || !*list)
    return;

  /* special handling for list head */
  if (data == *list) {
    *list = data->next;
    free(data);
    return;
  }

  osd_data_t *it = *list;
  while (it) {
    if (data == it->next) {
      it->next = data->next;
      free(data);
      return;
    }
    it = it->next;
  }
}

/*
 * osd_data_init()
 *
 *  - allocate and fill new osd_data_t
 *
 */
static osd_data_t *osd_data_init(vo_overlay_t *ovl, osd_data_t *next,
                                 uint32_t factor_x, uint32_t factor_y)
{
  osd_data_t *data = calloc(1, sizeof(osd_data_t));

  data->source = ovl;
  data->next   = next;

  memcpy(&data->ovl, ovl, sizeof(vo_overlay_t));

  int num_rle = data->ovl.num_rle;

  /* new position and size */
  int x2 = ovl->x + ovl->width + 1;
  int y2 = ovl->y + ovl->height + 1;
  x2  = ((x2+1) * factor_x) >> 16;
  y2  = ((y2+1) * factor_y) >> 16;
  data->ovl.x       = (ovl->x * factor_x) >> 16;
  data->ovl.y       = (ovl->y * factor_y) >> 16;
  data->ovl.width   = x2 - data->ovl.x - 1;
  data->ovl.height  = y2 - data->ovl.y - 1;

  data->ovl.rle     = (rle_elem_t*)
    rle_scale_nearest((struct xine_rle_elem_s*)ovl->rle, &num_rle,
                      ovl->width, ovl->height,
                      data->ovl.width, data->ovl.height);
  data->ovl.num_rle = num_rle;
  data->scaled      = 1;

  LOGOSD("I: %d,%d %dx%d", ovl->x, ovl->y, ovl->width, ovl->height);
  LOGOSD("O: %d,%d %dx%d", data->ovl.x, data->ovl.y, data->ovl.width, data->ovl.height);

  return data;
}

/*
 *  osdscaler_hook_t
 */
typedef struct {
  vo_driver_hook_t h;

  /* configuration */
  uint8_t  enable;
  uint16_t osd_width;     /* the size of original full-screen OSD */
  uint16_t osd_height;

  uint8_t  unscaled_supported;

  /* current output */
  uint16_t output_width;
  uint16_t output_height;
  uint32_t factor_x;      /* scaling factor << 16 */
  uint32_t factor_y;
  uint16_t x_move;        /* OSD displacement */
  uint16_t y_move;

  /* currently showing OSDs - pre-scaled data */
  osd_data_t *active_osds;

} osdscaler_hook_t;

/*
 *
 */

/*
 * override overlay_begin()
 */
static void osdscaler_overlay_begin (vo_driver_t *self, vo_frame_t *frame, int changed)
{
  osdscaler_hook_t *this = (osdscaler_hook_t*)self;

  /* assume we're wired when called */
  if (!this->h.orig_driver) {
    LOGMSG("osdscaler_overlay_begin: assertion this->h.orig_driver failed !");
    abort();
  }

  /* re-scale all if OSD changed */
  if (changed) {
    LOGOSD("osdscaler_overlay_begin: changed = 1");
    osd_data_clear(this->active_osds);
    this->active_osds = NULL;
    this->unscaled_supported = (vo_def_get_capabilities(self) & VO_CAP_UNSCALED_OVERLAY);
  }

  /* redirect */
  if (this->h.orig_driver->overlay_begin)
    this->h.orig_driver->overlay_begin(this->h.orig_driver, frame, changed);
}

static int check_for_scaling(osdscaler_hook_t *this, vo_frame_t *frame, vo_overlay_t *overlay)
{
  this->x_move = this->y_move = 0;

  if (!this->enable)
    return 0;

  /* check for VDR OSD */
  if (overlay->hili_rgb_clut != VDR_OSD_MAGIC /* not from vdr */) {
    LOGOSD("overlay: source not VDR");
    return 0;
  }

  /* detect output size */
  if (overlay->unscaled && this->unscaled_supported) {
    this->output_width  = vo_def_get_property((vo_driver_t*)this, VO_PROP_WINDOW_WIDTH);
    this->output_height = vo_def_get_property((vo_driver_t*)this, VO_PROP_WINDOW_HEIGHT);
  } else {
    this->output_width  = frame->width;
    this->output_height = frame->height;
    /* check cropping */
    if (frame->crop_top    > 0) this->output_height -= frame->crop_top;
    if (frame->crop_bottom > 0) this->output_height -= frame->crop_bottom;
    if (frame->crop_left   > 0) this->output_width  -= frame->crop_left;
    if (frame->crop_right  > 0) this->output_width  -= frame->crop_right;
  }

  /* check if scaling should be done */
  if (ABS(this->output_width  - this->osd_width)  > this->osd_width /20 ||
      ABS(this->output_height - this->osd_height) > this->osd_height/20 ) {
    LOGOSD("scaling required");
    this->factor_x = 0x10000 * this->output_width  / this->osd_width;
    this->factor_y = 0x10000 * this->output_height / this->osd_height;
    return 1;
  }

  /* if no scaling was required, we may still need to re-center OSD */
  if(this->output_width != this->osd_width)
    this->x_move = (this->output_width - this->osd_width)/2;
  if(this->output_height != this->osd_height)
    this->y_move = (this->output_height - this->osd_height)/2;

  return 0;
}

static vo_overlay_t *scale_overlay(osdscaler_hook_t *this, vo_frame_t *frame, vo_overlay_t *overlay)
{
  if (check_for_scaling(this, frame, overlay)) {

    /* find old scaled OSD */
    osd_data_t *scaled = this->active_osds;
    while (scaled && scaled->source != overlay)
      scaled = scaled->next;

    /* output size changed since last scaling (need to re-scale) ? */
    if (scaled &&
        (scaled->output_width  != this->output_width ||
         scaled->output_height != this->output_height)) {

      LOGOSD("re-scaling required: output size changed %dx%d -> %dx%d",
             scaled->output_width, scaled->output_height, this->output_width, this->output_height);

      osd_data_remove(&this->active_osds, scaled);
      scaled = NULL;
    }

    /* not scaled found ? */
    if (!scaled) {
      LOGOSD("scaling OSD");
      scaled = this->active_osds = osd_data_init(overlay, this->active_osds,
                                                 this->factor_x, this->factor_y);
      scaled->output_width  = this->output_width;
      scaled->output_height = this->output_height;
    }

    /* override */
    overlay = &scaled->ovl;
  }

  return overlay;
}

/*
 * interface
 */

/*
 * override overlay_blend()
 */
static void osdscaler_overlay_blend (vo_driver_t *self, vo_frame_t *frame, vo_overlay_t *overlay)
{
  osdscaler_hook_t *this = (osdscaler_hook_t*)self;

  overlay = scale_overlay(this, frame, overlay);

  /* redirect */
  if (this->h.orig_driver->overlay_blend)
    this->h.orig_driver->overlay_blend(this->h.orig_driver, frame, overlay);
}

/*
 * override overlay_end()
 */
static void osdscaler_overlay_end (vo_driver_t *self, vo_frame_t *vo_img)
{
  osdscaler_hook_t *this = (osdscaler_hook_t*)self;

  /* redirect */
  if (this->h.orig_driver->overlay_end)
    this->h.orig_driver->overlay_end(this->h.orig_driver, vo_img);
}


/*
 * Management interface
 */

/*
 * override get_capabilities()
 */
static uint32_t osdscaler_get_capabilities(vo_driver_t *self)
{
  return vo_def_get_capabilities(self) |
         VO_CAP_OSDSCALING;
}

/*
 * override get_property()
 */
static int osdscaler_get_property(vo_driver_t *self, int prop)
{
  osdscaler_hook_t *this = (osdscaler_hook_t*)self;

  switch (prop) {
    case VO_PROP_OSD_SCALING: return this->enable;
    case VO_PROP_OSD_WIDTH:   return this->osd_width;
    case VO_PROP_OSD_HEIGHT:  return this->osd_height;
    default:;
  }

  return vo_def_get_property(self, prop);
}

/*
 * override set_property()
 */
static int osdscaler_set_property(vo_driver_t *self, int prop, int val)
{
  osdscaler_hook_t *this = (osdscaler_hook_t*)self;

  switch (prop) {
    case VO_PROP_OSD_SCALING:
      if (this->enable != val) {
        LOGOSD("osdscaler_set_property: enable %d->%d", this->enable, val);
        this->enable = val?1:0;
      }
      return this->enable;

    case VO_PROP_OSD_WIDTH:
      if (this->osd_width != val) {
        LOGOSD("osdscaler_set_property: width  %d->%d", this->osd_width, val);
        this->osd_width  = val & 0xfff;
      }
      return this->osd_width;

    case VO_PROP_OSD_HEIGHT:
      if (this->osd_height != val) {
        LOGOSD("osdscaler_set_property: height %d->%d", this->osd_height, val);
        this->osd_height = val & 0xfff;
      }
      return this->osd_height;
  }

  return vo_def_set_property(self, prop, val);
}


/*
 * dispose()
 */
static void osdscaler_dispose(vo_driver_t *self)
{
  osdscaler_hook_t *this = (osdscaler_hook_t *) self;

  osd_data_clear(this->active_osds);

  vo_def_dispose(self);
}


/*
 * init()
 */
vo_driver_t *osdscaler_init(void)
{
  osdscaler_hook_t *this = calloc(1, sizeof(osdscaler_hook_t));

  /* OSD interface */
  this->h.vo.overlay_begin    = osdscaler_overlay_begin;
  this->h.vo.overlay_blend    = osdscaler_overlay_blend;
  this->h.vo.overlay_end      = osdscaler_overlay_end;

  /* management interface */
  this->h.vo.get_capabilities = osdscaler_get_capabilities;
  this->h.vo.get_property     = osdscaler_get_property;
  this->h.vo.set_property     = osdscaler_set_property;

  this->h.vo.dispose          = osdscaler_dispose;

  /* initialize data */
  this->enable     = 0;
  this->osd_width  = 720;
  this->osd_height = 576;

  return &this->h.vo;
}
