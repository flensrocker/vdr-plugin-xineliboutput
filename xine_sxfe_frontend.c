/*
 * xine_sxfe_frontend.c: Simple front-end, X11 functions
 *
 * See the main source file 'xineliboutput.c' for copyright information and
 * how to reach the author.
 *
 * $Id: xine_sxfe_frontend.c,v 1.2 2006-06-04 11:00:04 phintuka Exp $
 *
 */

#define HAVE_XF86VIDMODE
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
#include <linux/unistd.h> /* gettid() */

#include <X11/Xlib.h>
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
  xine_audio_port_t       *audio_port;
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

  /* frontend */
  int                      playback_finished;

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
#ifdef HAVE_XF86VIDMODE
  /* XF86VidMode Extension */
  XF86VidModeModeInfo**  XF86_modelines;
  int                    XF86_modelines_count;
#endif

  int                      completion_event;

  int                      xpos, ypos;
  int                      width, height;

  Atom                     wm_del_win;
  Atom                     sxfe_interrupt;

} fe_t, sxfe_t;


/* Common (non-X11/FB) frontend functions */
#include "xine_frontend.c"


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

  Atom       prop;
  MWMHints   mwmhints;
  XSizeHints hint;
  double     res_h, res_v, aspect_diff;

  if(this->display)
    this->fe.fe_display_close(this_gen);

  if(keyfunc)
    this->keypress = keyfunc;

  LOGDBG("sxfe_display_open(width=%d, height=%d, fullscreen=%d, display=%s)",
	 width, height, fullscreen, video_port);

  this->xpos            = 0;
  this->ypos            = 0;
  this->width           = width;
  this->height          = height;

  this->fullscreen      = fullscreen;
  this->vmode_switch    = modeswitch;
  this->aspect          = aspect;
  this->cropping        = 0;
  this->field_order     = field_order ? 1 : 0;
  this->scale_video     = scale_video;
  strcpy(this->modeline, modeline);

  /*
   * init x11 stuff
   */

  if (!XInitThreads ()) {
    LOGERR("sxfe_display_open: XInitThreads failed");
    free(this);
    return 0;
  }

  if(video_port && strlen(video_port)>3) {
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
    free(this);
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

  /* no border in fullscreen window */
  prop = XInternAtom(this->display, "_MOTIF_WM_HINTS", False);
  mwmhints.flags = MWM_HINTS_DECORATIONS;
  mwmhints.decorations = 0;
  XChangeProperty(this->display, this->window[1], prop, prop, 32,
		  PropModeReplace, (unsigned char *) &mwmhints,
		  PROP_MWM_HINTS_ELEMENTS);

  XSelectInput (this->display, this->window[0],
		StructureNotifyMask |
		ExposureMask |
		KeyPressMask );
  XSelectInput (this->display, this->window[1],
		StructureNotifyMask |
		ExposureMask |
		KeyPressMask );
  
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
  this->wm_del_win = XInternAtom(this->display, "WM_DELETE_WINDOW", False);
  this->sxfe_interrupt = XInternAtom(this->display, "SXFE_INTERRUPT", False);

  XSetWMProtocols(this->display, this->window[fullscreen], &(this->wm_del_win), 1);

  /* no cursor */
  XDefineCursor(this->display, this->window[0], None);
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
    XDefineCursor(this->display, this->window[0], no_ptr);
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
  
  if(this->width != width || this->height != height) {
    this->width      = width;
    this->height     = height;
    this->fullscreen = fullscreen;

    if(!fullscreen) {    
      XLockDisplay(this->display);
      XResizeWindow(this->display, this->window[0], this->width, this->height);
      XUnlockDisplay(this->display);
      if(!fullscreen && !this->fullscreen)
	xine_gui_send_vo_data(this->stream, XINE_GUI_SEND_DRAWABLE_CHANGED,
			      (void*) this->window[0]);
    }
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
    XMapRaised(this->display, this->window[this->fullscreen]);
    if(!fullscreen)
      XResizeWindow(this->display, this->window[0], this->width, this->height);    
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
  ev2.message_type = this->sxfe_interrupt;
  ev2.format  = 32;

  if(!XSendEvent(ev2.display, ev2.window, TRUE, /*KeyPressMask*/0, (XEvent *)&ev2))
    LOGERR("sxfe_interrupt: XSendEvent(ClientMessage) FAILED\n");

  XFlush(this->display);
}

static int sxfe_run(frontend_t *this_gen) 
{
  sxfe_t *this = (sxfe_t*)this_gen;

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
    if(poll(pfd, 1, 500) < 1 || !(pfd[0].revents & POLLIN)) {
      return 1;
    }
  }
  
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
	  XTranslateCoordinates(cev->display, cev->window,
				DefaultRootWindow(cev->display),
				0, 0, &this->xpos, &this->ypos, &tmp_win);
	  XUnlockDisplay(cev->display);
	} else {
	  this->xpos = cev->x;
	  this->ypos = cev->y;
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
#ifdef FE_STANDALONE
	  if(/*ks == XK_q || ks == XK_Q ||*/ ks == XK_Escape)
	    keep_going = 0;
	  else if(this->input || find_input(this))
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
	if ( cmessage->message_type == this->sxfe_interrupt )
	  LOGDBG("ClientMessage: sxfe_interrupt");

	if ( cmessage->data.l[0] == this->wm_del_win )
	  /* we got a window deletion message from out window manager.*/
	  keep_going=0;
      }
  }

  if (event.type == this->completion_event)
    xine_gui_send_vo_data (this->stream, XINE_GUI_SEND_COMPLETION_EVENT, &event);

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

#ifndef FE_STANDALONE
  this->fe.grab                  = fe_grab;
  this->fe.xine_osd_command      = xine_osd_command;
  this->fe.xine_control          = xine_control;

  this->fe.xine_queue_pes_packet = xine_queue_pes_packet;
#endif /*#ifndef FE_STANDALONE */
  
  return (frontend_t*)this;
}

/* ENTRY POINT */
const fe_creator_f fe_creator __attribute__((visibility("default"))) = sxfe_get_frontend;



