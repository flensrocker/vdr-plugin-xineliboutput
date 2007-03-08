/*
 * xine_sxfe_frontend.c: Simple front-end, X11 functions
 *
 * See the main source file 'xineliboutput.c' for copyright information and
 * how to reach the author.
 *
 * $Id: xine_sxfe_frontend.c,v 1.19 2007-03-08 13:12:55 phintuka Exp $
 *
 */

/*#define HAVE_XF86VIDMODE*/
#define HAVE_XDPMS

#include <errno.h>
#include <inttypes.h>
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
#include <poll.h>

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/keysym.h>
#include <X11/extensions/XShm.h>
#include <X11/Xutil.h>
#ifdef HAVE_XF86VIDMODE
#  include <X11/extensions/xf86vmode.h>
#endif
#ifdef HAVE_XDPMS
#  include <X11/extensions/dpms.h>
#endif
#ifdef HAVE_XV_FIELD_ORDER
#  include <X11/extensions/Xvlib.h>
#endif

#ifdef boolean
#  define HAVE_BOOLEAN
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

#define MWM_HINTS_DECORATIONS       (1L << 1)
#define PROP_MWM_HINTS_ELEMENTS     5
typedef struct _mwmhints {
  uint32_t                          flags;
  uint32_t                          functions;
  uint32_t                          decorations;
  int32_t                           input_mode;
  uint32_t                          status;
} MWMHints;


/*
 * data
 */

typedef struct sxfe_s {

  /* function pointers */
  frontend_t              fe;
  void (*update_display_size)(frontend_t*);

  /* xine stuff */
  xine_t                  *xine;
  xine_stream_t           *stream;
  input_plugin_t          *input;
  xine_video_port_t       *video_port;
  xine_video_port_t       *video_port_none;
  xine_audio_port_t       *audio_port;
  xine_audio_port_t       *audio_port_none;
  xine_event_queue_t      *event_queue;

  post_plugins_t          *postplugins;

  char                     configfile[256];

  int                      xine_visual_type;
  x11_visual_t             vis;

  int                      pes_buffers;
  int                      aspect;
  int                      cropping;
  int                      scale_video;
  int                      priority;


  double                   display_ratio;
  int                      overscan;

  /* frontend */
  int                      playback_finished;
  int                      slave_playback_finished;

  /* vdr */
  fe_keypress_f          keypress;

  /* X11 */
  Display                 *display;
  int                      screen;
  Window                   window[2];
  int                      fullscreen;
  int                      vmode_switch;
  int                      field_order;
  char                     modeline[256];

  int                      fullscreen_state_forced;

#ifdef HAVE_XF86VIDMODE
  /* XF86VidMode Extension */
  XF86VidModeModeInfo**  XF86_modelines;
  int                    XF86_modelines_count;
#endif

  int     completion_event;

  int     xpos, ypos;
  int     origxpos, origypos;
  int     width, height;
  int     origwidth, origheight;
  int     stay_above;
  int     no_border;

  Atom    atom_wm_delete_window;
  Atom    atom_sxfe_interrupt;

  Atom    atom_wm_hints, atom_win_layer;

  Atom    atom_state;
  Atom    atom_state_add, atom_state_del;
  Atom    atom_state_above, atom_state_fullscreen, atom_state_on_top;

  int     video_width, video_height;

} fe_t, sxfe_t;


/* Common (non-X11/FB) frontend functions */
#include "xine_frontend.c"

#include "vdrlogo_32x32.c"

#define DOUBLECLICK_TIME   500  // ms


static void fe_dest_size_cb (void *data,
			     int video_width, int video_height, double video_pixel_aspect,
			     int *dest_width, int *dest_height, double *dest_pixel_aspect)  
{ 
  fe_t *this = (fe_t *)data;
  
  if (!this)
    return;

  *dest_width  = this->width;
  *dest_height = this->height;

  *dest_pixel_aspect = fe_dest_pixel_aspect(this, video_pixel_aspect,
					    video_width, video_height);
}

static void set_fullscreen_props(sxfe_t *this)
{
  XEvent ev;

  if(this->atom_state == None) {
    this->atom_win_layer        = XInternAtom(this->display, "_WIN_LAYER", False);
    this->atom_state            = XInternAtom(this->display, "_NET_WM_STATE", False);
    this->atom_state_add        = XInternAtom(this->display, "_NET_WM_STATE_ADD", False);
    this->atom_state_del        = XInternAtom(this->display, "_NET_WM_STATE_DEL", False);
    this->atom_state_above      = XInternAtom(this->display, "_NET_WM_STATE_ABOVE", False);
    this->atom_state_fullscreen = XInternAtom(this->display, "_NET_WM_STATE_FULLSCREEN", False);
    this->atom_state_on_top     = XInternAtom(this->display, "_NET_WM_STATE_STAYS_ON_TOP", False);
  }

  memset(&ev, 0, sizeof(ev));
  ev.type                 = ClientMessage;
  ev.xclient.type         = ClientMessage;
  ev.xclient.message_type = this->atom_state;
  ev.xclient.display      = this->display;
  ev.xclient.window       = this->window[1];
  ev.xclient.format       = 32;
  ev.xclient.data.l[0]    = 1; 
  /*ev.xclient.data.l[0]    = this->atom_state_add;*/

  /* _NET_WM_STATE_FULLSCREEN */
  XLockDisplay(this->display);
  ev.xclient.data.l[1] = this->atom_state_fullscreen;
  XSendEvent(this->display, DefaultRootWindow(this->display), False, 
	     SubstructureNotifyMask|SubstructureRedirectMask, &ev);
  XUnlockDisplay(this->display);

  /* _NET_WM_STATE_ABOVE */
  XLockDisplay(this->display);
  ev.xclient.data.l[1] = this->atom_state_above;
  XSendEvent(this->display, DefaultRootWindow(this->display), False, 
	     SubstructureNotifyMask|SubstructureRedirectMask, &ev);
  XUnlockDisplay(this->display);

  /* _NET_WM_STATE_ON_TOP */
  XLockDisplay(this->display);
  ev.xclient.data.l[1] = this->atom_state_on_top;
  XSendEvent(this->display, DefaultRootWindow(this->display), False, 
	     SubstructureNotifyMask|SubstructureRedirectMask, &ev);
  XUnlockDisplay(this->display);
}

static void set_border(sxfe_t *this, int border)
{
  MWMHints   mwmhints;

  this->no_border = border ? 0 : 1;

  /* Set/remove border */
  this->atom_wm_hints = XInternAtom(this->display, "_MOTIF_WM_HINTS", False);
  mwmhints.flags = MWM_HINTS_DECORATIONS;
  mwmhints.decorations = this->no_border ? 0 : 1;
  XChangeProperty(this->display, this->window[0], this->atom_wm_hints, this->atom_wm_hints, 32,
		  PropModeReplace, (unsigned char *) &mwmhints,
		  PROP_MWM_HINTS_ELEMENTS);
}

static void set_above(sxfe_t *this, int stay_above)
{
  XEvent ev;
  long propvalue[1];

  this->stay_above = stay_above;

  XStoreName(this->display, this->window[0], stay_above ? "VDR (top)" : "VDR");

  memset(&ev, 0, sizeof(ev));
  ev.type                 = ClientMessage;
  ev.xclient.type         = ClientMessage;
  ev.xclient.message_type = this->atom_state;
  ev.xclient.display      = this->display;
  ev.xclient.window       = this->window[0];
  ev.xclient.format       = 32;
  ev.xclient.data.l[0]    = stay_above ? 1:0; /*this->atom_state_add : this->atom_state_del;*/

  /* _NET_WM_STATE_ABOVE */
  XLockDisplay(this->display);
  ev.xclient.data.l[1] = this->atom_state_above;
  XSendEvent(this->display, DefaultRootWindow(this->display), False, 
	     SubstructureNotifyMask|SubstructureRedirectMask, &ev);
  XUnlockDisplay(this->display);

  /* _NET_WM_STATE_ON_TOP */
  XLockDisplay(this->display);
  ev.xclient.data.l[1] = this->atom_state_on_top;
  XSendEvent(this->display, DefaultRootWindow(this->display), False, 
	     SubstructureNotifyMask|SubstructureRedirectMask, &ev);
  XUnlockDisplay(this->display);

  /* _NET_WM_STATE_STICKY */
  XLockDisplay(this->display);
  ev.xclient.data.l[1] = XInternAtom(this->display, "_NET_WM_STATE_STICKY", False);
  XSendEvent(this->display, DefaultRootWindow(this->display), False, 
	     SubstructureNotifyMask|SubstructureRedirectMask, &ev);
  XUnlockDisplay(this->display);

  /* _WIN_LAYER */
  propvalue[0] = stay_above ? 10 : 6;
  XLockDisplay(this->display);
  XChangeProperty(this->display, this->window[0], XInternAtom(this->display, "_WIN_LAYER", False),
		  XA_CARDINAL, 32, PropModeReplace, (unsigned char *)propvalue,
		  1);
  XUnlockDisplay(this->display);

#if 0
  /* sticky */
  memset(&ev, 0, sizeof(ev));
  ev.xclient.type         = ClientMessage;
  ev.xclient.message_type = XInternAtom(this->display, "_WIN_STATE", False);
  ev.xclient.display      = this->display;
  ev.xclient.window       = this->window[0];
  ev.xclient.format       = 32;
  ev.xclient.data.l[0] = (1<<0);
  ev.xclient.data.l[1] = (stay_above?(1<<0):0);
  XSendEvent(this->display, DefaultRootWindow(this->display), False, 
	     SubstructureNotifyMask|SubstructureRedirectMask, &ev);
#endif
#if 0
  /* on top */
  memset(&ev, 0, sizeof(ev));
  ev.xclient.type         = ClientMessage;
  ev.xclient.message_type = XInternAtom(this->display, "_WIN_LAYER", False);
  ev.xclient.display      = this->display;
  ev.xclient.window       = this->window[0];
  ev.xclient.format       = 32;
  ev.xclient.data.l[0] = (stay_above?10:6);
  XSendEvent(this->display, DefaultRootWindow(this->display), False, 
	     SubstructureNotifyMask|SubstructureRedirectMask, &ev);
#endif
#if 0
  /* layer */
  XClientMessageEvent xev;

  memset(&xev, 0, sizeof(xev));
  xev.type = ClientMessage;
  xev.display = this->display;
  xev.window = this->window[0];
  xev.message_type = this->atom_win_layer;
  xev.format = 32;
  xev.data.l[0] = 10;
  xev.data.l[1] = CurrentTime;
  XSendEvent(this->display, DefaultRootWindow(this->display), False, 
	     SubstructureNotifyMask, (XEvent *) & xev);

  XMapRaised(this->display, this->window[0]);
#endif
#if 0
  xine_gui_send_vo_data(this->stream, XINE_GUI_SEND_DRAWABLE_CHANGED,
			(void*) this->window[this->fullscreen]);
#endif
}

/*
 * sxfe_display_open
 *
 * connect to X server, create windows
 */

static int sxfe_display_open(frontend_t *this_gen, int width, int height, int fullscreen,
			     int modeswitch, const char *modeline, int aspect,
			     fe_keypress_f keyfunc, const char *video_port,
			     int scale_video, int field_order) 
{
  sxfe_t    *this = (sxfe_t*)this_gen;

  MWMHints   mwmhints;
  XSizeHints hint;
  double     res_h, res_v, aspect_diff;

  if(this->display)
    this->fe.fe_display_close(this_gen);

  if(keyfunc) {
    this->keypress = keyfunc;
    this->keypress("XKeySym", ""); /* triggers learning mode */
  }

  LOGDBG("sxfe_display_open(width=%d, height=%d, fullscreen=%d, display=%s)",
	 width, height, fullscreen, video_port);

  this->xpos            = 0;
  this->ypos            = 0;
  this->origxpos        = 0;
  this->origypos        = 0;
  this->width           = width;
  this->height          = height;
  this->origwidth       = width>0 ? width : 720;
  this->origheight      = height>0 ? height : 576;

  this->fullscreen      = fullscreen;
  this->vmode_switch    = modeswitch;
  this->aspect          = aspect;
  this->cropping        = 0;
  this->field_order     = field_order ? 1 : 0;
  this->scale_video     = scale_video;
  this->overscan        = 0;
  this->fullscreen_state_forced = 0;
  strcpy(this->modeline, modeline);

  /*
   * init x11 stuff
   */

  if (!XInitThreads ()) {
    LOGERR("sxfe_display_open: XInitThreads failed");
    free(this);
    return 0;
  }

  if(video_port && strlen(video_port)>2) {
    if(!(this->display = XOpenDisplay(video_port)))
      LOGERR("sxfe_display_open: failed to connect to X server (%s)",
	     video_port);
  }
  if(!this->display) {
    if(NULL!=(video_port=getenv("DISPLAY")) && !(this->display = XOpenDisplay(video_port)))
      LOGERR("sxfe_display_open: failed to connect to X server (%s)",
	     video_port);
  }
  if(!this->display) {
    video_port = "127.0.0.1:0.0";
    if(!(this->display = XOpenDisplay(video_port)))
      LOGERR("sxfe_display_open: failed to connect to X server (%s)",
	     video_port);
  }
  if(!this->display) {
    this->display = XOpenDisplay(NULL);
  }
  if (!this->display) {
    LOGERR("sxfe_display_open: failed to connect to X server");
    /*free(this);*/
    return 0;
  }

  XLockDisplay (this->display);

  this->screen = DefaultScreen(this->display);

  /* #warning sxfe_display_open: TODO: switch vmode */

  /* completion event */
  if (XShmQueryExtension (this->display) == True) {
    this->completion_event = XShmGetEventBase (this->display) + ShmCompletion;
  } else {
    this->completion_event = -1;
  }

  if(fullscreen) {
    this->width = DisplayWidth(this->display, this->screen);
    this->height = DisplayHeight(this->display, this->screen);
  }
  
  /* create and display our video window */
  this->window[0] = XCreateSimpleWindow (this->display,
					 DefaultRootWindow(this->display),
					 this->xpos, this->ypos,
					 this->width, this->height,
					 1, 0, 0);
  this->window[1] = XCreateSimpleWindow(this->display, XDefaultRootWindow(this->display),
					0, 0, (DisplayWidth(this->display, this->screen)),
					(DisplayHeight(this->display, this->screen)), 0, 0, 0);

  hint.flags  = USSize | USPosition | PPosition | PSize;
  hint.x      = 0;
  hint.y      = 0;
  hint.width  = DisplayWidth(this->display, this->screen);
  hint.height = DisplayHeight(this->display, this->screen);
  XSetNormalHints(this->display, this->window[1], &hint);

  /* full-screen window */
  set_fullscreen_props(this);

  /* no border in fullscreen window */
  this->atom_wm_hints = XInternAtom(this->display, "_MOTIF_WM_HINTS", False);
  mwmhints.flags = MWM_HINTS_DECORATIONS;
  mwmhints.decorations = 0;
  XChangeProperty(this->display, this->window[1], this->atom_wm_hints, this->atom_wm_hints, 32,
		  PropModeReplace, (unsigned char *) &mwmhints,
		  PROP_MWM_HINTS_ELEMENTS);

  /* Select input */
  XSelectInput (this->display, this->window[0],
		StructureNotifyMask |
		ExposureMask |
		KeyPressMask | 
		ButtonPressMask | ButtonReleaseMask | ButtonMotionMask);
  XSelectInput (this->display, this->window[1],
		StructureNotifyMask |
		ExposureMask |
		KeyPressMask | 
		ButtonPressMask);

  /* Window name */
  XStoreName(this->display, this->window[0], "VDR");
  XStoreName(this->display, this->window[1], "VDR");

  /* Icon */
  {
    XChangeProperty(this->display, this->window[0],
		    XInternAtom(this->display, "_NET_WM_ICON", False),
		    XA_CARDINAL, 32, PropModeReplace,
		    (unsigned char *) &vdrlogo_32x32, 
		    2 + vdrlogo_32x32.width*vdrlogo_32x32.height);
  }

  /* Map current window */
  XMapRaised (this->display, this->window[this->fullscreen]);
  
  /* determine display aspect ratio */
  res_h = (DisplayWidth  (this->display, this->screen)*1000
	   / DisplayWidthMM (this->display, this->screen));
  res_v = (DisplayHeight (this->display, this->screen)*1000
	   / DisplayHeightMM (this->display, this->screen));
  this->display_ratio = res_v / res_h;
  aspect_diff = this->display_ratio - 1.0;
  if ((aspect_diff < 0.01) && (aspect_diff > -0.01)) {
    this->display_ratio   = 1.0;
  }
  LOGDBG("Display size : %d x %d mm", 
	 DisplayWidthMM (this->display, this->screen),
	 DisplayHeightMM (this->display, this->screen));
  LOGDBG("               %d x %d pixels", 
	 DisplayWidth (this->display, this->screen),
	 DisplayHeight (this->display, this->screen));
  LOGDBG("               %ddpi / %ddpi", 
	 (int)(res_v/1000*25.4), (int)(res_h/1000*25.4));
  LOGDBG("Display ratio: %f/%f = %f", res_v, res_h, this->display_ratio);

  /* we want to get notified if user closes the window */
  this->atom_wm_delete_window = XInternAtom(this->display, "WM_DELETE_WINDOW", False);
  XSetWMProtocols(this->display, this->window[fullscreen], &(this->atom_wm_delete_window), 1);

  /* no cursor */
#if 0
  XDefineCursor(this->display, this->window[0], None);
#endif
  XDefineCursor(this->display, this->window[1], None);
  {
    static char bm_no_data[] = { 0,0,0,0, 0,0,0,0 };
    Pixmap bm_no;
    Cursor no_ptr;
    XColor black, dummy;
    bm_no = XCreateBitmapFromData(this->display, this->window[fullscreen], bm_no_data, 8, 8);
    XAllocNamedColor(this->display, DefaultColormapOfScreen(DefaultScreenOfDisplay(this->display)), 
		     "black", &black, &dummy);
    no_ptr = XCreatePixmapCursor(this->display, bm_no, bm_no, &black, &black, 0, 0);
#if 0
    XDefineCursor(this->display, this->window[0], no_ptr);
#endif
    XDefineCursor(this->display, this->window[1], no_ptr);
  }

  XUnlockDisplay (this->display);

  /* No screen saver */
  /* #warning TODO: suspend --> activate blank screen saver / DPMS display off ? */
  XSetScreenSaver(this->display, 0, 0, DefaultBlanking, DefaultExposures);
#ifdef HAVE_XDPMS
  {
    int dpms_dummy;
    if (DPMSQueryExtension(this->display, &dpms_dummy, &dpms_dummy) && DPMSCapable(this->display)) {
/*    DPMSInfo(dpy, &dpms_state, &dpms_on); */
      DPMSDisable(this->display);
    }
  }
#endif

  this->xine_visual_type     = XINE_VISUAL_TYPE_X11;
  this->vis.display          = this->display;
  this->vis.screen           = this->screen;
  this->vis.d                = this->window[this->fullscreen];
  this->vis.dest_size_cb     = fe_dest_size_cb;
  this->vis.frame_output_cb  = fe_frame_output_cb;
  this->vis.user_data        = this;

  this->atom_sxfe_interrupt  = XInternAtom(this->display, "SXFE_INTERRUPT", False);

  set_fullscreen_props(this);

  return 1;
}

/*
 * sxfe_display_config
 *
 * configure windows
 */
static int sxfe_display_config(frontend_t *this_gen, 
			       int width, int height, int fullscreen, 
			       int modeswitch, const char *modeline, 
			       int aspect, int scale_video, 
			       int field_order) 
{
  sxfe_t *this = (sxfe_t*)this_gen;
  
  if(this->fullscreen_state_forced)
    fullscreen = this->fullscreen;

  if(!fullscreen && (this->width != width || this->height != height)) {
    this->width      = width;
    this->height     = height;

    XLockDisplay(this->display);
    XResizeWindow(this->display, this->window[0], this->width, this->height);
    XUnlockDisplay(this->display);
    if(!fullscreen && !this->fullscreen)
      xine_gui_send_vo_data(this->stream, XINE_GUI_SEND_DRAWABLE_CHANGED,
			    (void*) this->window[0]);
  }

  if(fullscreen) {
    this->width = DisplayWidth(this->display, this->screen);
    this->height = DisplayHeight(this->display, this->screen);
  }
  
  if(fullscreen != this->fullscreen) {
    Window    tmp_win;
    XLockDisplay(this->display);
    XUnmapWindow(this->display, this->window[this->fullscreen]);
    this->fullscreen = fullscreen;
    if(fullscreen)
      set_fullscreen_props(this);
    else
      set_above(this, this->stay_above);
    XMapRaised(this->display, this->window[this->fullscreen]);
    if(!fullscreen) {
      XResizeWindow(this->display, this->window[0], this->width, this->height);
      XMoveWindow(this->display, this->window[0], this->xpos, this->ypos);
      set_above(this, this->stay_above);
    } else {
      set_fullscreen_props(this);
    }
    XSync(this->display, False);
    XTranslateCoordinates(this->display, this->window[this->fullscreen],
			  DefaultRootWindow(this->display),
			  0, 0, &this->xpos, &this->ypos, &tmp_win);
    XUnlockDisplay(this->display);
    xine_gui_send_vo_data(this->stream, XINE_GUI_SEND_DRAWABLE_CHANGED,
			  (void*) this->window[this->fullscreen]);
  }

  if(!modeswitch && strcmp(modeline, this->modeline)) {
    strcpy(this->modeline, modeline);
    /* #warning TODO - switch vmode */
  }

  this->vmode_switch = modeswitch;
  this->aspect = aspect;
  this->scale_video = scale_video;
#ifdef HAVE_XV_FIELD_ORDER
  if(this->field_order != field_order) {
    if(XInternAtom(this->display, "XV_SWAP_FIELDS", True) != None)
      XvSetPortAttribute (this->display, 53, 
			  XInternAtom (this->display, "XV_SWAP_FIELDS", False), 
			  field_order);
  }
#endif
  this->field_order = field_order ? 1 : 0;
  
  return 1;
}

static void sxfe_toggle_fullscreen(sxfe_t *this)
{
  int force = this->fullscreen_state_forced;
  this->fullscreen_state_forced = 0;

  if(!this->fullscreen) {
    this->origwidth  = this->width;
    this->origheight = this->height;
    this->origxpos = this->xpos;
    this->origypos = this->ypos;
  } else {
    this->xpos = this->origxpos;
    this->ypos = this->origypos;
  }

  this->fe.fe_display_config((frontend_t*)this, this->origwidth, this->origheight,
			     this->fullscreen ? 0 : 1, 
			     this->vmode_switch, this->modeline, 
			     this->aspect, this->scale_video, this->field_order);
  
  this->fullscreen_state_forced = !force;
}

/*
 *   X event loop
 */

static void sxfe_interrupt(frontend_t *this_gen) 
{
  sxfe_t *this = (sxfe_t*)this_gen;
  XClientMessageEvent ev2;

  ev2.type    = ClientMessage;
  ev2.display = this->display;
  ev2.window  = this->window[this->fullscreen];
  ev2.message_type = this->atom_sxfe_interrupt;
  ev2.format  = 32;

  if(!XSendEvent(ev2.display, ev2.window, TRUE, /*KeyPressMask*/0, (XEvent *)&ev2))
    LOGERR("sxfe_interrupt: XSendEvent(ClientMessage) FAILED\n");

  XFlush(this->display);
}

static int sxfe_run(frontend_t *this_gen) 
{
  sxfe_t *this = (sxfe_t*)this_gen;
  static int dragging = 0, drx = 0, dry = 0;

  int keep_going = 1;
  XEvent event;

  /* poll X server (connection socket). 
     (XNextEvent will block if no events are queued).
     We want to use timeout, blocking for long time usually causes vdr
     watchdog to emergency exit ... */
  if (! XPending(this->display)) {
    struct pollfd pfd[2];
    pfd[0].fd = ConnectionNumber(this->display);
    pfd[0].events = POLLIN;
    if(poll(pfd, 1, 50) < 1 || !(pfd[0].revents & POLLIN)) {
      return 1;
    }
  }

  while(keep_going && XPending(this->display) > 0) {

    XNextEvent (this->display, &event);   
    
    switch (event.type) {
      case Expose:
	if (event.xexpose.count == 0)
	  xine_gui_send_vo_data (this->stream, XINE_GUI_SEND_EXPOSE_EVENT, &event);
	break;
	
      case ConfigureNotify:
      {
	XConfigureEvent *cev = (XConfigureEvent *) &event;
	Window tmp_win;
	
	this->width  = cev->width;
	this->height = cev->height;
	
	if ((cev->x == 0) && (cev->y == 0)) {
	  XLockDisplay(cev->display);
	  if(!this->fullscreen) 
	    XTranslateCoordinates(cev->display, cev->window,
				  DefaultRootWindow(cev->display),
				  0, 0, &this->xpos, &this->ypos, &tmp_win);
	  XUnlockDisplay(cev->display);
	} else {
	  if(!this->fullscreen) {
	    this->xpos = cev->x;
	    this->ypos = cev->y;
	  }
	}
	break;
      }

      case ButtonRelease:
      {
	dragging = 0;
	break;
      }

      case MotionNotify:
      {
	if(dragging && !this->fullscreen) {
	  XMotionEvent *mev = (XMotionEvent *) &event;
	  Window tmp_win;
	  int xpos, ypos;

	  XLockDisplay(this->display);

	  while(XCheckMaskEvent(this->display, ButtonMotionMask, &event));

	  XTranslateCoordinates(this->display, this->window[0],
				DefaultRootWindow(this->display),
				0, 0, &xpos, &ypos, &tmp_win);

	  this->xpos = (xpos += mev->x_root - drx);
	  this->ypos = (ypos += mev->y_root - dry);
	  drx = mev->x_root;
	  dry = mev->y_root;

	  XMoveWindow(this->display, this->window[0], xpos, ypos);

	  XUnlockDisplay(this->display);
	}
	break;
      }

      case ButtonPress:
      {
	XButtonEvent *bev = (XButtonEvent *) &event;
	if(bev->button == Button1) {
	  static Time prev_time = 0;
	  if(bev->time - prev_time < DOUBLECLICK_TIME) {
	    /* Toggle fullscreen */
	    sxfe_toggle_fullscreen(this);
	    prev_time = 0; /* don't react to third click ... */
	  } else {
	    prev_time = bev->time;
	    if(!this->fullscreen && this->no_border && !dragging) {
	      dragging = 1;
	      drx = bev->x_root;
	      dry = bev->y_root;
	    }
	  }
	} else if(bev->button == Button3) {
	  if(!this->fullscreen) {
	    if(!this->stay_above) {
	      set_above(this, 1);
	    } else if(!this->no_border) {
	      set_border(this, 0);
	    } else {
	      set_border(this, 1);
	      set_above(this, 0);
	    }
	  }
	}
	break;
      }

      case KeyPress:
      case KeyRelease:
      {
	XKeyEvent *kevent = (XKeyEvent *) &event;
	KeySym          ks;
	char            *ksname;
	char            buffer[20];
	int             buf_len = 20;
	XComposeStatus  status;

	if(kevent->keycode) {
	  XLookupString(kevent, buffer, buf_len, &ks, &status);
	  ksname = XKeysymToString(ks);
#ifdef XINELIBOUTPUT_FE_TOGGLE_FULLSCREEN
	  if(ks == XK_f || ks == XK_F) {
	    sxfe_toggle_fullscreen(this);
	  } else if(ks == XK_d || ks == XK_D) {
	    xine_set_param(this->stream, XINE_PARAM_VO_DEINTERLACE, 
			   xine_get_param(this->stream, XINE_PARAM_VO_DEINTERLACE) ? 0 : 1);
	  } else
#endif
#ifdef FE_STANDALONE
	  if(/*ks == XK_q || ks == XK_Q ||*/ ks == XK_Escape) {
	    terminate_key_pressed = 1;
	    keep_going = 0;
	  } else if(this->input || find_input(this))
	    process_xine_keypress(this->input, "XKeySym",ksname, 0, 0);
#else
	  if(this->keypress) 
	    this->keypress("XKeySym",ksname);
#endif
	}
      }
      break;
      
      case ClientMessage:
      {
	XClientMessageEvent *cmessage = (XClientMessageEvent *) &event;	
	if ( cmessage->message_type == this->atom_sxfe_interrupt )
	  LOGDBG("ClientMessage: sxfe_interrupt");

	if ( cmessage->data.l[0] == this->atom_wm_delete_window )
	  /* we got a window deletion message from out window manager.*/
	  LOGDBG("ClientMessage: WM_DELETE_WINDOW");
	  keep_going=0;
      }
    }
    
    if (event.type == this->completion_event)
      xine_gui_send_vo_data (this->stream, XINE_GUI_SEND_COMPLETION_EVENT, &event);
  }

  return keep_going;
}

static void sxfe_display_close(frontend_t *this_gen) 
{
  sxfe_t *this = (sxfe_t*)this_gen;

  if(this && this->display) {
    
    if(this->xine)
      this->fe.xine_exit(this_gen);
    
    XLockDisplay(this->display);
    XUnmapWindow(this->display, this->window[this->fullscreen]);
    XDestroyWindow(this->display, this->window[0]);
    XDestroyWindow(this->display, this->window[1]);
    XUnlockDisplay(this->display);
    XCloseDisplay (this->display);
    this->display = NULL;
  }
}

static frontend_t *sxfe_get_frontend(void)
{
  sxfe_t *this = malloc(sizeof(sxfe_t));
  memset(this, 0, sizeof(sxfe_t));
  
  this->fe.fe_display_open   = sxfe_display_open;
  this->fe.fe_display_config = sxfe_display_config;
  this->fe.fe_display_close  = sxfe_display_close;
  
  this->fe.xine_init  = fe_xine_init;
  this->fe.xine_open  = fe_xine_open;
  this->fe.xine_play  = fe_xine_play;
  this->fe.xine_stop  = fe_xine_stop;
  this->fe.xine_close = fe_xine_close;
  this->fe.xine_exit  = fe_xine_exit;
  this->fe.xine_is_finished = fe_is_finished;
  
  this->fe.fe_run  = sxfe_run;
  this->fe.fe_interrupt = sxfe_interrupt;
  this->fe.fe_free = fe_free;

  this->fe.grab                  = fe_grab;
#ifndef FE_STANDALONE
  this->fe.xine_osd_command      = xine_osd_command;
  this->fe.xine_control          = xine_control;

  this->fe.xine_queue_pes_packet = xine_queue_pes_packet;
#endif /*#ifndef FE_STANDALONE */
  
  return (frontend_t*)this;
}

/* ENTRY POINT */
const fe_creator_f fe_creator __attribute__((visibility("default"))) = sxfe_get_frontend;



