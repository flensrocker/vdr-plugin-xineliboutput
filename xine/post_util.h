/*
 * post_util.h: post plugin utility functions
 *
 * See the main source file 'xineliboutput.c' for copyright information and
 * how to reach the author.
 *
 * $Id: post_util.h,v 1.1 2008-02-19 01:24:19 phintuka Exp $
 *
 */

/*
 * class util prototypes
 */

static void *init_plugin(xine_t *xine, void *data);
static char *get_identifier(post_class_t *class_gen);
static char *get_description(post_class_t *class_gen);
static void  class_dispose(post_class_t *class_gen);

/* required from plugin: */
static post_plugin_t *open_plugin(post_class_t *class_gen, int inputs,
				  xine_audio_port_t **audio_target,
				  xine_video_port_t **video_target);

/*
 * plugin util prototypes
 */

static int   dispatch_draw(vo_frame_t *frame, xine_stream_t *stream);
static int   intercept_frame_yuy(post_video_port_t *port, vo_frame_t *frame);
static int   post_draw(vo_frame_t *frame, xine_stream_t *stream);

/* required from plugin: */
static vo_frame_t    *got_frame(vo_frame_t *frame);
static void           draw_internal(vo_frame_t *frame, vo_frame_t *new_frame);


/*
 * class utils
 */

static void *init_plugin(xine_t *xine, void *data)
{
  post_class_t *class = (post_class_t *)malloc(sizeof(post_class_t));
  
  if (!class)
    return NULL;

  class->open_plugin     = open_plugin;
  class->get_identifier  = get_identifier;
  class->get_description = get_description;
  class->dispose         = class_dispose;

  return class;
}

static char *get_identifier(post_class_t *class_gen)
{
  return PLUGIN_ID;
}

static char *get_description(post_class_t *class_gen)
{
  return PLUGIN_DESCR;
}

static void class_dispose(post_class_t *class_gen)
{
  free(class_gen);
}

/*
 * plugin utils
 */

static int dispatch_draw(vo_frame_t *frame, xine_stream_t *stream)
{
  int skip;
  _x_post_frame_copy_down(frame, frame->next);
  skip = frame->next->draw(frame->next, stream);
  _x_post_frame_copy_up(frame, frame->next);
  return skip;
}

static int intercept_frame_yuy(post_video_port_t *port, vo_frame_t *frame)
{
  return (frame->format == XINE_IMGFMT_YV12 || frame->format == XINE_IMGFMT_YUY2);
}

static int post_draw(vo_frame_t *frame, xine_stream_t *stream)
{
  vo_frame_t *new_frame;
  int skip;

  if (frame->bad_frame)
    return dispatch_draw(frame, stream);

  new_frame = got_frame(frame);

  if (!new_frame)
    return dispatch_draw(frame, stream);

  _x_post_frame_copy_down(frame, new_frame);

  draw_internal(frame, new_frame);

  skip = new_frame->draw(new_frame, stream);
  _x_post_frame_copy_up(frame, new_frame);
  new_frame->free(new_frame);

  return skip;
}

