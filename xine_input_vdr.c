/*
 * xine_input_vdr.c: xine VDR input plugin
 *
 * See the main source file 'xineliboutput.c' for copyright information and
 * how to reach the author.
 *
 * $Id: xine_input_vdr.c,v 1.28 2006-08-18 04:16:52 phintuka Exp $
 *
 */


#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#define XINE_ENGINE_INTERNAL
#define METRONOM_CLOCK_INTERNAL

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <poll.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <errno.h>
#include <sys/time.h>
#include <dlfcn.h>
#include <linux/unistd.h> /* gettid() */
#include <sys/resource.h> /* setpriority() */
#include <syslog.h>

#include <xine/xine_internal.h>
#include <xine/xineutils.h>
#include <xine/input_plugin.h>
#include <xine/plugin_catalog.h>
#include <xine/io_helper.h>
#include <xine/buffer.h>
#include <xine/post.h>

#include "xine_input_vdr.h"
#include "xine_input_vdr_net.h"
#include "xine_osd_command.h"

/***************************** DEFINES *********************************/

/*#define LOG_UDP*/
/*#define LOG_OSD*/
/*#define LOG_CMD*/
/*#define LOG_SCR*/
/*#define LOG_TRACE*/

#define ADJUST_SCR_SPEED        1
#define METRONOM_PREBUFFER_VAL  (4 * 90000 / 25 )

#define HD_BUF_NUM_BUFS   (2048)  /* 2k payload * 2048 = 4Mb , ~ 1 second */
#define HD_BUF_ELEM_SIZE  (2048+64)

#define RADIO_MAX_BUFFERS  10

/******************************* LOG ***********************************/

#define LOG_MODULENAME "[input_vdr] "
#define SysLogLevel iSysLogLevel

#include "logdefs.h"

#if !defined(XINELIBOUTPUT_DEBUG_STDOUT) && \
    !defined(XINELIBOUTPUT_DEBUG_STDERR)
# undef  x_syslog
# define x_syslog syslog_with_tid
#endif

int iSysLogLevel  = 1;
int bLogToSysLog  = 0;
int bSymbolsFound = 0;

_syscall0(pid_t, gettid)

static void syslog_with_tid(int level, const char *fmt, ...)
{
  va_list argp;
  char buf[512];
  va_start(argp, fmt);
  vsnprintf(buf, 512, fmt, argp);
  if(!bLogToSysLog) {
    printf(LOG_MODULENAME "%s\n", buf);
  } else {
    syslog(level, "[%d] " LOG_MODULENAME "%s", gettid(), buf);
  }
  va_end(argp);
}

static void SetupLogLevel(void)
{
  void *lib = NULL;
  if( !(lib = dlopen (NULL, RTLD_LAZY | RTLD_GLOBAL))) {
    LOGERR("Can't dlopen self: %s", dlerror());
  } else {
    int *pLogToSyslog = (int*)dlsym(lib, "LogToSysLog");
    int *pSysLogLevel = (int*)dlsym(lib, "SysLogLevel");
    bLogToSysLog = pLogToSyslog && *pLogToSyslog;
    iSysLogLevel = pSysLogLevel ? (*pSysLogLevel) : 2;
    LOGDBG("Symbol SysLogLevel %s : value %d", 
	   pSysLogLevel ? "found" : "not found", iSysLogLevel);
    LOGDBG("Symbol LogToSysLog %s : value %s", 
	   pLogToSyslog ? "found" : "not found", bLogToSysLog ? "yes" : "no");
    bSymbolsFound = pSysLogLevel && pLogToSyslog;
    dlclose(lib);
  }
}


#ifdef LOG_SCR
#  define LOGSCR(x...) LOGMSG("SCR: " x)
#else
#  define LOGSCR(x...)
#endif
#
#ifdef LOG_OSD
#  define LOGOSD(x...) LOGMSG("OSD: " x)
#else
#  define LOGOSD(x...)
#endif
#
#ifdef LOG_UDP
#  define LOGUDP(x...) LOGMSG("UDP:" x)
#else
#  define LOGUDP(x...) 
#endif
#ifdef LOG_CMD
#  define LOGCMD(x...) LOGMSG("CMD:" x)
#else
#  define LOGCMD(x...) 
#endif
#ifdef LOG_TRACE
#  undef TRACE
#  define TRACE(x...) printf(x)
#else
#  undef TRACE
#  define TRACE(x...) 
#endif


/******************************* DATA ***********************************/

#define KILOBYTE(x)   (1024 * (x))

typedef struct pvrscr_s pvrscr_t;
typedef struct udp_data_s udp_data_t;

/* plugin class */
typedef struct vdr_input_class_s {
  input_class_t     input_class;
  xine_t           *xine;
} vdr_input_class_t;

/* input plugin */
typedef struct vdr_input_plugin_s {
  input_plugin_t      input_plugin;

  /* VDR */
  vdr_input_plugin_funcs_t funcs; 

  /* plugin */
  vdr_input_class_t  *cls;
  xine_stream_t      *stream;
  xine_event_queue_t *event_queue;

  char               *mrl;

  xine_stream_t      *pip_stream;
  xine_stream_t      *slave_stream;
  xine_event_queue_t *slave_event_queue;

  /* Sync */
  pthread_mutex_t     lock;
  pthread_mutex_t     vdr_entry_lock;

  /* Playback */
  int                 no_video;
  int                 live_mode;
  int                 still_mode;
  int                 playback_finished;
  int                 stream_start;
  int                 send_pts;
  int                 loop_play;
  int                 hd_stream;  /* true if current stream is HD */

  int                 audio_stream_id; /* ((PES PID) << 8) | (SUBSTREAM ID) */
  int                 reset_audio_cnt;
  int                 prev_audio_stream_id;

  /* SCR */
  pvrscr_t           *scr;
  int                 scr_tunning;
  int                 speed_before_pause;
  int                 is_paused;

  int                 I_frames;   /* amount of I-frames passed to demux */
  int                 B_frames;
  int                 P_frames;

  int64_t             pause_start;
  int                 paused_frames;

  /* Network */
  pthread_t           control_thread;
  pthread_t           data_thread;
  int                 threads_initialized;
  volatile int        control_running;
  volatile int        fd_data;
  volatile int        fd_control;
  int                 tcp, udp, rtp;
  buf_element_t      *read_buffer;   /* used when reading from socket */
  udp_data_t         *udp_data;
  int                 client_id;
  int                 token;

  /* buffer */
  buf_element_t      *curr_buffer;   /* used in read */
  fifo_buffer_t      *block_buffer;  /* blocks to be demuxed */
  fifo_buffer_t      *buffer_pool;   /* stream's video fifo */
  fifo_buffer_t      *big_buffer;    /* for jumbo PES */
  fifo_buffer_t      *hd_buffer;     /* more buffer for HD streams */
  fifo_buffer_t      *iframe_buffer; /* buffer for cached I-frame */
  int                 saving_iframe;
  uint64_t            discard_index; /* index of next byte to feed to demux; 
					all data before this offset will 
					be discarded */
  int                 discard_frame;
  uint64_t            guard_index;   /* data before this offset will not be discarded */
  int                 guard_frame;
  uint64_t            curpos;        /* current position (demux side) */
  int                 curframe;      
  int                 max_buffers;   /* max no. of non-demuxed buffers */

  /* saved video properties */
  int   video_properties_saved;
  int   orig_hue;
  int   orig_brightness;
  int   orig_saturation;
  int   orig_contrast;

  /* OSD */
  pthread_mutex_t osd_lock;
  int vdr_osd_width, vdr_osd_height;
  int video_width, video_height;
  int video_changed;
  int rescale_osd;
  int rescale_osd_downscale;
  int unscaled_osd;
  int unscaled_osd_opaque;
  int unscaled_osd_lowresvideo;
  int osdhandle[MAX_OSD_OBJECT];
  int64_t last_changed_vpts[MAX_OSD_OBJECT];
  osd_command_t osddata[MAX_OSD_OBJECT];

} vdr_input_plugin_t;


/***************************** UDP DATA *********************************/

struct udp_data_s {

  /* receiving queue for re-ordering and re-transmissions */
  buf_element_t *queue[UDP_SEQ_MASK+1];
  int queued;   /* count of frames in queue */

  /* expected sequence number of next incoming packet */
  int next_seq;

  /* for re-send requests */
  uint64_t queue_input_pos;  /* stream position of next incoming byte */
  int      resend_requested;
  int      queue_full_signaled;

  /* missing frames ratio statistics */
  int missed_frames;
  int received_frames; 

  /* SCR adjust */
  int scr_jump_done;

  /* Server address (used to validate incoming packets) */
  struct sockaddr_in server_address;
};

/* UDP sequence number handling */
#define NEXTSEQ(x)  ((x + 1) & UDP_SEQ_MASK)
#define PREVSEQ(x)  ((x + UDP_SEQ_MASK) & UDP_SEQ_MASK)
#define INCSEQ(x)   (x = NEXTSEQ(x))
#define ADDSEQ(x,n) ((x + UDP_SEQ_MASK + 1 + n) & UDP_SEQ_MASK)

#define UDP_SIGNAL_FULL_TRESHOLD     50  /* ~100ms with DVB mpeg2 
					    (2k-blocks @ 8 Mbps)  */
#define UDP_SIGNAL_NOT_FULL_TRESHOLD 100 /* ~200ms with DVB mpeg2 
					    (2k-blocks @ 8 Mbps)  */

static udp_data_t *init_udp_data(void)
{
  udp_data_t *data = (udp_data_t *)xine_xmalloc(sizeof(udp_data_t));

  memset(data->queue, 0, sizeof(data->queue));
  data->next_seq         = 0;
  data->queued           = 0;
  data->queue_input_pos  = 0ULL;
  data->resend_requested = 0;
  data->missed_frames    = 0;
  data->received_frames  = -1; 
  data->scr_jump_done    = 0;

  return data;
}

static void free_udp_data(udp_data_t *data)
{
  int i;

  for(i=0; i<=UDP_SEQ_MASK; i++)
    if(data->queue[i])
      data->queue[i]->free_buffer(data->queue[i]);

  free(data);
}

#if 0
static void flush_udp_data(udp_data_t *data)
{
  /* flush all data immediately even if there are gaps */
}
#endif

#ifdef ADJUST_SCR_SPEED
/******************************* SCR *************************************
 *
 * unix System Clock Reference + fine tunning
 *
 * pvrscr code is mostly copied from xine, input_pvr.c
 *
 * fine tunning is used to change playback speed in live mode
 * to keep in sync with mpeg source
 *************************************************************************/

struct pvrscr_s {
  scr_plugin_t     scr;

  struct timeval   cur_time;
  int64_t          cur_pts;
  int              xine_speed;
  double           speed_factor;
  double           speed_tunning;

  pthread_mutex_t  lock;

  struct timeval   last_time;
};

static int pvrscr_get_priority (scr_plugin_t *scr) 
{
  return 10; /* high priority */
}

/* Only call pvrscr_set_pivot when already mutex locked ! */
static void pvrscr_set_pivot (pvrscr_t *this) 
{
  struct   timeval tv;
  int64_t pts;
  double   pts_calc;

  xine_monotonic_clock(&tv,NULL);

  pts_calc = (tv.tv_sec  - this->cur_time.tv_sec) * this->speed_factor;
  pts_calc += (tv.tv_usec - this->cur_time.tv_usec) * this->speed_factor / 1e6;
  pts = this->cur_pts + pts_calc;

  /* This next part introduces a one off inaccuracy
   * to the scr due to rounding tv to pts.
   */
  this->cur_time.tv_sec=tv.tv_sec;
  this->cur_time.tv_usec=tv.tv_usec;
  this->cur_pts=pts;

  this->last_time.tv_sec  = tv.tv_sec;
  this->last_time.tv_usec = tv.tv_usec;

  return ;
}

static int pvrscr_set_fine_speed (scr_plugin_t *scr, int speed) 
{
  pvrscr_t *this = (pvrscr_t*) scr;

  pthread_mutex_lock (&this->lock);

  pvrscr_set_pivot( this );
  this->xine_speed     = speed;
  this->speed_factor   = (double) speed * 90000.0 / 
                         (1.0*XINE_FINE_SPEED_NORMAL) *
                         this->speed_tunning;

  pthread_mutex_unlock (&this->lock);

  return speed;
}

static void pvrscr_speed_tunning (pvrscr_t *this, double factor) 
{
  pthread_mutex_lock (&this->lock);

  pvrscr_set_pivot( this );
  this->speed_tunning = factor;
  this->speed_factor = (double) this->xine_speed * 90000.0 / 
                       (1.0*XINE_FINE_SPEED_NORMAL) *
                       this->speed_tunning;

  pthread_mutex_unlock (&this->lock);
}

static void pvrscr_skip_frame (pvrscr_t *this) 
{
  pthread_mutex_lock (&this->lock);

  pvrscr_set_pivot( this );
  this->cur_pts += (90000/25)*1ULL;

  pthread_mutex_unlock (&this->lock);
}

static void pvrscr_adjust (scr_plugin_t *scr, int64_t vpts) 
{
  pvrscr_t *this = (pvrscr_t*) scr;
  struct   timeval tv;

  pthread_mutex_lock (&this->lock);

  xine_monotonic_clock(&tv,NULL);
  this->cur_time.tv_sec=tv.tv_sec;
  this->cur_time.tv_usec=tv.tv_usec;
  this->cur_pts = vpts;

  this->last_time.tv_sec  = tv.tv_sec;
  this->last_time.tv_usec = tv.tv_usec;

  pthread_mutex_unlock (&this->lock);
}

static void pvrscr_start (scr_plugin_t *scr, int64_t start_vpts) 
{
  pvrscr_t *this = (pvrscr_t*) scr;

  pthread_mutex_lock (&this->lock);

  xine_monotonic_clock(&this->cur_time, NULL);
  this->cur_pts = start_vpts;

  this->last_time.tv_sec  = this->cur_time.tv_sec;
  this->last_time.tv_usec = this->cur_time.tv_usec;

  pthread_mutex_unlock (&this->lock);

  pvrscr_set_fine_speed (&this->scr, XINE_FINE_SPEED_NORMAL);
}

static int64_t pvrscr_get_current (scr_plugin_t *scr) 
{
  pvrscr_t *this = (pvrscr_t*) scr;

  struct   timeval tv;
  int64_t pts;
  double   pts_calc;
  pthread_mutex_lock (&this->lock);

  xine_monotonic_clock(&tv,NULL);

#ifdef LOG_SCR
  if(this->last_time.tv_sec+3 < tv.tv_sec && this->last_time.tv_sec) {
    LOGMSG("ERROR - CLOCK JUMPED FORWARDS ? "
	   "(pvrscr_get_current diff %d.%06d sec)\n",
           (int)(tv.tv_sec - this->last_time.tv_sec),
	   (int)(tv.tv_usec - this->last_time.tv_usec));
    pthread_mutex_unlock (&this->lock);
    pvrscr_adjust(scr,this->cur_pts);
    pthread_mutex_lock (&this->lock);
  }
  else if(this->last_time.tv_sec > tv.tv_sec) {
    LOGMSG("ERROR - CLOCK JUMPED BACKWARDS ! "
	   "(pvrscr_get_current diff %d.%06d sec)\n",
           (int)(tv.tv_sec - this->last_time.tv_sec),
	   (int)(tv.tv_usec - this->last_time.tv_usec));
    pthread_mutex_unlock (&this->lock);
    pvrscr_adjust(scr,this->cur_pts);
    pthread_mutex_lock (&this->lock);
  }
#endif

  pts_calc = (tv.tv_sec  - this->cur_time.tv_sec) * this->speed_factor;
  pts_calc += (tv.tv_usec - this->cur_time.tv_usec) * this->speed_factor / 1e6;
  
  pts = this->cur_pts + pts_calc;

  this->last_time.tv_sec  = tv.tv_sec;
  this->last_time.tv_usec = tv.tv_usec;

  pthread_mutex_unlock (&this->lock);

  return pts;
}

static void pvrscr_exit (scr_plugin_t *scr) 
{
  pvrscr_t *this = (pvrscr_t*) scr;

  pthread_mutex_destroy (&this->lock);
  free(this);
}

static pvrscr_t* pvrscr_init (void) 
{
  pvrscr_t *this;

  this = malloc(sizeof(*this));
  memset(this, 0, sizeof(*this));

  this->scr.interface_version = 3;
  this->scr.set_fine_speed    = pvrscr_set_fine_speed;
  this->scr.get_priority      = pvrscr_get_priority;
  this->scr.adjust            = pvrscr_adjust;
  this->scr.start             = pvrscr_start;
  this->scr.get_current       = pvrscr_get_current;
  this->scr.exit              = pvrscr_exit;

  pthread_mutex_init (&this->lock, NULL);

  pvrscr_speed_tunning(this, 1.0 );
  pvrscr_set_fine_speed (&this->scr, XINE_SPEED_PAUSE);

  LOGSCR("SCR init complete");

  return this;
}

/*
 *  SCR tunning 
 */

#define SCR_TUNNING_PAUSED -3
#define SCR_TUNNING_OFF     0

#ifdef LOG_SCR
static inline const char *scr_tunning_str(int value)
{
  switch(value) {
    case 2:  return "SCR +1.0%";
    case 1:  return "SCR +0.5%";
    case SCR_TUNNING_OFF:  return "SCR +0.0%";
    case -1: return "SCR -0.5%";
    case -2: return "SCR -1.0%";
    case SCR_TUNNING_PAUSED: return "SCR PAUSED";
    default: break;
  }
  return "ERROR";
}
#endif

static int64_t monotonic_time_ms (void) 
{
  static struct timeval tv_0;
  static int init_done = 0;
  struct timeval tv;
  int64_t ms;

  if(!init_done) {
    init_done = 1;
    xine_monotonic_clock(&tv_0, NULL);    
  }
  xine_monotonic_clock(&tv, NULL);

  ms = 1000LL * (tv.tv_sec - tv_0.tv_sec);
  ms += tv.tv_usec / 1000;
  return ms;
}

static void scr_tunning_set_paused(vdr_input_plugin_t *this)
{
  if(this->scr_tunning != SCR_TUNNING_PAUSED &&
     !this->slave_stream ) {
    this->scr_tunning = SCR_TUNNING_PAUSED;  /* marked as paused */
    if(this->scr)
      pvrscr_speed_tunning(this->scr, 1.0);
    
    this->speed_before_pause = _x_get_fine_speed(this->stream);

#if 0
    if(_x_get_fine_speed(this->stream) != XINE_SPEED_PAUSE)
      _x_set_fine_speed(this->stream, XINE_SPEED_PAUSE);
#else
    _x_set_fine_speed(this->stream, 1000000 / 1000); /* -> speed to 0.1%  */
#endif
    this->pause_start = monotonic_time_ms(); 
    this->paused_frames = 0;
  }
}

static void reset_scr_tunning(vdr_input_plugin_t *this, int new_speed)
{
  if(this->scr_tunning != SCR_TUNNING_OFF) {
    this->scr_tunning = SCR_TUNNING_OFF; /* marked as normal */
    if(this->scr) 
      pvrscr_speed_tunning(this->scr, 1.0);

    if(new_speed >= 0) {
      if(_x_get_fine_speed(this->stream) != new_speed) {
	_x_set_fine_speed(this->stream, XINE_FINE_SPEED_NORMAL);
      }
      pvrscr_set_fine_speed((scr_plugin_t*)this->scr, XINE_FINE_SPEED_NORMAL);
    }
  }
}

static void vdr_adjust_realtime_speed(vdr_input_plugin_t *this) 
{
  /* Grab current buffer usage */
  int num_used = this->buffer_pool->size(this->buffer_pool) + 
                 this->block_buffer->size(this->block_buffer);
  int num_free = this->buffer_pool->num_free(this->buffer_pool);
  int scr_tunning = this->scr_tunning;

  if(this->stream->audio_fifo)
    num_used += this->stream->audio_fifo->size(this->stream->audio_fifo);
  num_free -= (this->buffer_pool->buffer_pool_capacity - this->max_buffers);

#ifdef LOG_SCR
  {
    static int fcnt=0;
    if(!((fcnt++)%2500) || 
       (this->scr_tunning==SCR_TUNNING_PAUSED && !(fcnt%10)) ||
       (this->no_video        && !(fcnt%50))) {
      LOGSCR("Buffer %2d%% (%3d/%3d) %s", 
	     100*num_used/(num_used+num_free), 
	     num_used, num_used+num_free, 
	     scr_tunning_str(this->scr_tunning));
    }
  }

  if(this->scr_tunning==SCR_TUNNING_PAUSED) {
    if(_x_get_fine_speed(this->stream) != XINE_SPEED_PAUSE) { 
      LOGMSG("ERROR: SCR PAUSED ; speed=%d bool=%d", 
	     _x_get_fine_speed(this->stream), 
	     (int)_x_get_fine_speed(this->stream) == XINE_SPEED_PAUSE);
      _x_set_fine_speed(this->stream, XINE_SPEED_PAUSE);
    }
  }
#endif

  /* If buffer is (almost) empty. pause it for a while */
  if( num_used < 1 && 
      scr_tunning != SCR_TUNNING_PAUSED && 
      !this->no_video && !this->still_mode) {
/* 
   #warning TODO:
   - First I-frame can be delivered as soon as it is decoded 
   -> illusion of faster channel switches
   - Clock must still be paused, but stream can be in PLAYING state
   (if clock is not paused we will got a lot of discarded frames 
   as those are decoded too late according to running SCR)
*/
    int num_vbufs = this->stream->video_out->get_property(this->stream->video_out, 
							  VO_PROP_BUFS_IN_FIFO);
    if(num_vbufs < 3) {
      LOGSCR("SCR paused by adjust_speed (vbufs=%d)", num_vbufs);
      scr_tunning_set_paused(this);
    } else {
      LOGSCR("adjust_speed: no pause, enough vbufs queued");
    }

  /* If currently paused, revert to normal if buffer > 50% */
  } else if( scr_tunning == SCR_TUNNING_PAUSED) {
/* 
   #warning TODO:
   - Using amount of buffers is not good trigger as it depends on channel bitrate
   - Wait time is not not good trigger as it depends on tuner lock time
   -> maybe keep track of PTSes or wait until decoder has complete IBBBP frame sequence ? 
   - First I-frame can be delivered as soon as it is decoded 
   -> illusion of faster channel switches
*/
    int num_vbufs = this->stream->video_out->get_property(this->stream->video_out, 
							  VO_PROP_BUFS_IN_FIFO);
    this->paused_frames++;

    if( num_used/2 > num_free 
	|| (this->no_video && num_used > 5)
	|| this->paused_frames > 200
        || (this->paused_frames > 100 
	    && this->pause_start + 400 < monotonic_time_ms())
	|| num_vbufs > 5
	|| this->still_mode
	) {

      LOGSCR("SCR tunning resetted by adjust_speed, "
	     "vbufs=%d (SCR was paused for %d bufs/%d ms)",
	     num_vbufs, this->paused_frames, 
	     monotonic_time_ms() - this->pause_start);

      this->paused_frames = 0; 
      this->pause_start = 0;
      reset_scr_tunning(this, this->speed_before_pause);
    }

  /* when playing realtime, adjust the scr to make xine buffers half full */
  } else if( _x_get_fine_speed(this->stream) == XINE_FINE_SPEED_NORMAL) {

    if(this->no_video) {  /* radio stream ? */
      if( num_used >= (RADIO_MAX_BUFFERS-1))
        scr_tunning = +1; /* play faster */
      else if( num_used <= (RADIO_MAX_BUFFERS/3))
        scr_tunning = -1; /* play slower */
      else
        scr_tunning = SCR_TUNNING_OFF;
    } else {
      if( num_used > 4*num_free )
        scr_tunning = +2; /* play 1% faster */
      else if( num_used > 2*num_free )
        scr_tunning = +1; /* play .5% faster */
      else if( num_free > 4*num_used ) /* <20% */
        scr_tunning = -2; /* play 1% slower */
      else if( num_free > 2*num_used ) /* <33% */
        scr_tunning = -1; /* play .5% slower */
      else if( (scr_tunning > 0 && num_free > num_used) ||
	       (scr_tunning < 0 && num_used > num_free) )
        scr_tunning = SCR_TUNNING_OFF;
    }

    if( scr_tunning != this->scr_tunning ) {
      LOGSCR("scr_tunning: %s -> %s (buffer %d/%d)", 
	     scr_tunning_str(this->scr_tunning), 
	     scr_tunning_str(scr_tunning), num_used, num_free );
      this->scr_tunning = scr_tunning;

      /* make it play .5% / 1% faster or slower */
      if(this->scr)
	pvrscr_speed_tunning(this->scr, 1.0 + (0.005 * scr_tunning) );
    }

  /* If replay mode or trick speed mode, switch tunning off */
  } else if( this->scr_tunning ) {
    reset_scr_tunning(this, -1);
  }
}

#else /* ADJUST_SCR_SPEED */

struct pvrscr_s { 
  int dummy; 
};

static void vdr_adjust_realtime_speed(vdr_input_plugin_t *this, 
				      fifo_buffer_t *fifo1, 
				      fifo_buffer_t *fifo2, 
				      int speed ) 
{
}

static void reset_scr_tunning(vdr_input_plugin_t *this, int new_speed) 
{
}

static void scr_tunning_set_paused(vdr_input_plugin_t *this, 
				   int speed_before_pause) 
{
}

#endif /* ADJUST_SCR_SPEED */

/******************************* TOOLS ***********************************/

#define MIN(a,b) ((a)<(b)?(a):(b))
#define MAX(a,b) ((a)>(b)?(a):(b))

static int64_t pts_from_pes(const uint8_t *buf, int size)
{
  int64_t pts = -1;
  if(size>14 && buf[7] & 0x80) { /* pts avail */
    pts  = ((int64_t)( buf[ 9] & 0x0E)) << 29;
    pts |=  (int64_t)( buf[10]         << 22 );
    pts |=  (int64_t)((buf[11] & 0xFE) << 14 );
    pts |=  (int64_t)( buf[12]         <<  7 );
    pts |=  (int64_t)((buf[13] & 0xFE) >>  1 );
  }
  return pts;
}

static void create_timeout_time(struct timespec *abstime, int timeout_ms)
{
  struct timeval now;
  gettimeofday(&now, NULL);
  now.tv_usec += timeout_ms * 1000;
  while (now.tv_usec >= 1000000) {   /* take care of an overflow */
    now.tv_sec++;
    now.tv_usec -= 1000000;
  }
  abstime->tv_sec = now.tv_sec;
  abstime->tv_nsec = now.tv_usec * 1000;
}

static int io_select_rd (int fd) 
{
  fd_set fdset, eset;
  int ret;
  struct timeval select_timeout;

  if(fd < 0)
    return XIO_ERROR;

  FD_ZERO (&fdset);
  FD_ZERO (&eset);
  FD_SET  (fd, &fdset);
  FD_SET  (fd, &eset);
    
  select_timeout.tv_sec  = 0;
  select_timeout.tv_usec = 500*1000; /* 500 ms */
  errno = 0;
  ret = select (fd + 1, &fdset, NULL, &eset, &select_timeout);

  if (ret == 0)
    return XIO_TIMEOUT;
  if (ret < 0) {
    if(errno == EINTR || errno == EAGAIN)
      return XIO_TIMEOUT;
    return XIO_ERROR;
  }
  if(FD_ISSET(fd,&eset))
    return XIO_ERROR;
  if(FD_ISSET(fd,&fdset))
    return XIO_READY;

  return XIO_TIMEOUT; /* newer reached ... */
}

static void write_control(vdr_input_plugin_t *this, const char *str)
{
  size_t len = (size_t)strlen(str);
  size_t ret;

  do {
    if(this->fd_control < 0)
      return;
    errno = 0;
    if(len != (ret = write(this->fd_control, str, len))) {
      /*if(ret <= 0 && (errno == EINTR || errno == EAGAIN))*/
      if(ret <= 0 && errno == EAGAIN)
	continue;
      if(ret <= 0 && errno == EINTR)
	LOGERR("write_control failed (%d) EINTR", ret);
      else
	LOGERR("write_control failed (%d)", ret);
      close(this->fd_control);
      this->fd_control = -1;
    }
  } while(0);
}

static void printf_control(vdr_input_plugin_t *this, const char *fmt, ...)
{
  va_list argp;
  char buf[512];
  va_start(argp, fmt);
  vsnprintf(buf, 512, fmt, argp);
  write_control(this, buf);
  va_end(argp);
}

static int readline_control(vdr_input_plugin_t *this, char *buf, int maxlen)
{
  int num_bytes = 0, total_bytes = 0, err;

  *buf = 0;
  while(total_bytes < maxlen-1 ) {

    pthread_testcancel();
    err = io_select_rd(this->fd_control);
    pthread_testcancel();

    if(this->fd_control < 0)
      return -1;

    if(err == XIO_TIMEOUT) 
      continue;   
    if(err == XIO_ABORTED) {
      LOGERR("readline_control: XIO_ABORTED at [%d]", num_bytes);
      continue;
    }
    if(err != XIO_READY /* == XIO_ERROR */) {
      LOGERR("readline_control: poll error at [%d]", num_bytes);
      return -1;
    }

    num_bytes = read (this->fd_control, buf + total_bytes, 1);
    pthread_testcancel();

    if (num_bytes <= 0) {
      LOGERR("readline_control: read error at [%d]", num_bytes);
      if(num_bytes < 0 && 
	 (errno == EINTR || errno==EAGAIN) && 
	 this->fd_control >= 0) {
	continue;
      }
      return -1;
    }
      
    if(buf[total_bytes]) {
      if(buf[total_bytes] == '\r') {
	buf[total_bytes] = 0;
      } else if(buf[total_bytes] == '\n') {
	buf[total_bytes] = 0;
	break;
      } else {
	total_bytes ++;
	buf[total_bytes] = 0;
      }
    }
    TRACE("readline_control: %d bytes ... %s\n", 
	  total_bytes, buf);
  }

  TRACE("readline_control: %d bytes (max %d)\n", total_bytes, maxlen);

  return total_bytes;
}


static int read_control(vdr_input_plugin_t *this, uint8_t *buf, int len)
{
  int num_bytes, total_bytes = 0, err;

  while(total_bytes < len) {
    pthread_testcancel();
    err = io_select_rd(this->fd_control);
    pthread_testcancel();

    if(this->fd_control < 0)
      return -1;

    if(err == XIO_TIMEOUT) {
      continue;
    }
    if(err == XIO_ABORTED) {
      LOGERR("read_control: XIO_ABORTED");
      continue;
    }
    if(err == XIO_ERROR) {
      LOGERR("read_control: poll error");
      return -1;
    }

    num_bytes = read (this->fd_control, buf + total_bytes, len - total_bytes);
    pthread_testcancel();

    if (num_bytes <= 0) {
      LOGERR("read_control read() error"); 
      return -1;
    }
    total_bytes += num_bytes;
  }

  return total_bytes;
}

static char *FindSubFile(const char *fname)
{
  char *subfile = (char*)malloc(strlen(fname)+4), *dot;
  strcpy(subfile, fname);
  dot = strrchr(subfile, '.');
  if(dot) {
    /*while(dot+1 > subfile) {*/
      struct stat st;
      strcpy(dot, ".sub");
      if (stat(subfile, &st) == 0)
	return subfile;
      strcpy(dot, ".srt");
      if (stat(subfile, &st) == 0)
	return subfile;
      strcpy(dot, ".txt");
      if (stat(subfile, &st) == 0)
	return subfile;
    /*  dot--; */
    /*}*/
  }
  free(subfile);
  return NULL;
}

static void queue_nosignal(vdr_input_plugin_t *this)
{
#define extern static
#include "nosignal_720x576.c"
#undef extern
  buf_element_t *buf;
  int pos = 0;
  while(pos < v_mpg_nosignal_length) {
    buf = this->stream->video_fifo->buffer_pool_try_alloc(this->stream->video_fifo);
    buf->content = buf->mem;
    buf->size = MIN(v_mpg_nosignal_length - pos, buf->max_size);
    buf->type = BUF_VIDEO_MPEG;
    xine_fast_memcpy(buf->content, &v_mpg_nosignal[pos], buf->size);
    pos += buf->size;
    this->stream->video_fifo->put(this->stream->video_fifo, buf);
  }
}

/************************** BUFFER HANDLING ******************************/

static void buffer_pool_free (buf_element_t *element) 
{
  fifo_buffer_t *this = (fifo_buffer_t *) element->source;

  pthread_mutex_lock (&this->buffer_pool_mutex);

  element->next = this->buffer_pool_top;
  this->buffer_pool_top = element;

  this->buffer_pool_num_free++;
  if (this->buffer_pool_num_free > this->buffer_pool_capacity) {
    LOGERR("xine-lib:buffer: There has been a fatal error: TOO MANY FREE's");
    _x_abort();
  }
  
  if(this->buffer_pool_num_free > 20)
    pthread_cond_signal (&this->buffer_pool_cond_not_empty);

  pthread_mutex_unlock (&this->buffer_pool_mutex);
}

#if 0
static buf_element_t *buffer_pool_timed_alloc(fifo_buffer_t *fifo, int timeoutMs, int buffer_limit)
{
  struct timespec    abstime;
  create_timeout_time(&abstime, timeoutMs);

  pthread_mutex_lock(&fifo->buffer_pool_mutex);

  while(fifo->buffer_pool_num_free <= buffer_limit) {
    if(pthread_cond_timedwait (&fifo->buffer_pool_cond_not_empty, &fifo->buffer_pool_mutex, &abstime) == ETIMEDOUT)
      break;
  }

  pthread_mutex_unlock(&fifo->buffer_pool_mutex);

  return fifo->buffer_pool_try_alloc(fifo);
}
#endif

#if 0
static buf_element_t *fifo_buffer_timed_get(fifo_buffer_t *fifo, int timeoutMs)
{
  struct timespec    abstime;
  create_timeout_time(&abstime, timeoutMs);
  
  return NULL;
}
#endif

static buf_element_t *fifo_buffer_try_get(fifo_buffer_t *fifo)
{
  int i;
  buf_element_t *buf;

  pthread_mutex_lock (&fifo->mutex);

  if (fifo->first==NULL) {
    pthread_mutex_unlock (&fifo->mutex);
    return NULL;
  }

  buf = fifo->first;

  fifo->first = fifo->first->next;
  if (fifo->first==NULL)
    fifo->last = NULL;

  fifo->fifo_size--;
  fifo->fifo_data_size -= buf->size;

  for(i = 0; fifo->get_cb[i]; i++)
    fifo->get_cb[i](fifo, buf, fifo->get_cb_data[i]);

  pthread_mutex_unlock (&fifo->mutex);

  return buf;
}

static int fifo_buffer_clear(fifo_buffer_t *fifo)
{
  int bytes = 0;
  buf_element_t *buf, *next, *prev=NULL;
  pthread_mutex_lock (&fifo->mutex);
  buf = fifo->first;
  while (buf != NULL) {
    next = buf->next;
    if ((buf->type & BUF_MAJOR_MASK) !=  BUF_CONTROL_BASE) {
      /* remove this buffer */
      if (prev)
	prev->next = next;
      else
	fifo->first = next;

      if (!next)
	fifo->last = prev;

      fifo->fifo_size--;
      bytes += buf->size;
      fifo->fifo_data_size -= buf->size;
      
      buf->free_buffer(buf);
    } else {
      prev = buf;
    }
    buf = next;
  }
  pthread_mutex_unlock (&fifo->mutex);

  return bytes;
}

static void signal_buffer_pool_not_empty(vdr_input_plugin_t *this)
{
  if(this->buffer_pool) {
    pthread_mutex_lock(&this->buffer_pool->buffer_pool_mutex);
    pthread_cond_broadcast(&this->buffer_pool->buffer_pool_cond_not_empty);
    pthread_mutex_unlock(&this->buffer_pool->buffer_pool_mutex);
  }
}

static void signal_buffer_not_empty(vdr_input_plugin_t *this)
{
  if(this->block_buffer) {
    pthread_mutex_lock(&this->block_buffer->mutex);
    pthread_cond_broadcast(&this->block_buffer->not_empty);
    pthread_mutex_unlock(&this->block_buffer->mutex);
  }
}

static buf_element_t *get_buf_element(vdr_input_plugin_t *this, int size, int force)
{
  buf_element_t *buf = NULL;

  /* HD buffer */
  if(this->hd_stream) {
    if(!this->hd_buffer)
      this->hd_buffer = _x_fifo_buffer_new(HD_BUF_NUM_BUFS, HD_BUF_ELEM_SIZE);

    if(size <= HD_BUF_ELEM_SIZE && this->hd_buffer && this->hd_stream)
      buf = this->hd_buffer->buffer_pool_try_alloc(this->hd_buffer);
  } else {
    if(this->hd_buffer) {
      LOGMSG("hd_buffer still exists ...");
      if(this->hd_buffer->num_free(this->hd_buffer) == this->hd_buffer->buffer_pool_capacity) {
	LOGMSG("disposing hd_buffer ...");
	this->hd_buffer->dispose(this->hd_buffer);
	this->hd_buffer = NULL;
      }
    }
  }

  /* limit max. buffered data */
  if(!force) {
    int buffer_limit = this->buffer_pool->buffer_pool_capacity - this->max_buffers;
    if(this->buffer_pool->buffer_pool_num_free < buffer_limit) 
      return NULL;
  }

  /* get smallest possible buffer */
  if(!buf) {
    if(size < 8000)
      buf = this->buffer_pool->buffer_pool_try_alloc(this->buffer_pool);
    else if(size < 0xffff) {
      buf = this->block_buffer->buffer_pool_try_alloc(this->block_buffer);
      LOGDBG("vdr_plugin_write: big PES (%d bytes) !", size);
    }
    else { /* len>64k */
      if(!this->big_buffer)
	this->big_buffer = _x_fifo_buffer_new(4,512*1024);
      buf = this->big_buffer->buffer_pool_try_alloc(this->big_buffer);
      LOGDBG("vdr_plugin_write: jumbo PES (%d bytes) !", size);
    }
  }

  buf->content = buf->mem;
  buf->size = 0;
  buf->type = BUF_DEMUX_BLOCK;

  buf->free_buffer = buffer_pool_free;

  return buf;
}

static void strip_network_headers(vdr_input_plugin_t *this, buf_element_t *buf)
{
  if(buf->type == BUF_MAJOR_MASK) {
    if(this->udp||this->rtp) {
      stream_udp_header_t *header = (stream_udp_header_t *)buf->content;
      this->curpos = header->pos;
      buf->content += sizeof(stream_udp_header_t);
      buf->size -= sizeof(stream_udp_header_t);
    } else {
      stream_tcp_header_t *header = (stream_tcp_header_t *)buf->content;
      this->curpos = header->pos;
      buf->content += sizeof(stream_tcp_header_t);
      buf->size -= sizeof(stream_tcp_header_t);
    }
    buf->type = BUF_DEMUX_BLOCK;
  }
}

static buf_element_t *make_padding_frame(vdr_input_plugin_t *this)
{
  static const uint8_t padding[] = {0x00,0x00,0x01,0xBE,0x00,0x02,0xff,0xff};
  buf_element_t *buf;

  buf = get_buf_element(this, 8, 1);
  if(!buf)
    buf = this->stream->audio_fifo->buffer_pool_try_alloc(this->stream->audio_fifo);

  if(buf) {
    buf->size = 8;
    buf->type = BUF_DEMUX_BLOCK;
    memcpy(buf->content, padding, 8);
  }

  return buf;
}

void put_control_buf(fifo_buffer_t *buffer, fifo_buffer_t *pool, int cmd)
{
  buf_element_t *buf = pool->buffer_pool_try_alloc(pool);
  if(buf) {
    buf->type = cmd;
    buffer->put(buffer, buf);
  }
}


/*************************** slave input (PIP stream) ********************/

typedef struct fifo_input_plugin_s {
  input_plugin_t      i;
  vdr_input_plugin_t *master;
  xine_stream_t      *stream;
  fifo_buffer_t      *buffer;
  fifo_buffer_t      *buffer_pool;
  off_t               pos;
} fifo_input_plugin_t;

static int   fifo_open(input_plugin_t *this_gen)
{ return 1; }
static uint32_t fifo_get_capabilities (input_plugin_t *this_gen) 
{ return INPUT_CAP_BLOCK; }
static uint32_t fifo_get_blocksize (input_plugin_t *this_gen) 
{ return 2 * 2048; }
static off_t fifo_get_current_pos (input_plugin_t *this_gen)
{ return -1; }
static off_t fifo_get_length (input_plugin_t *this_gen) 
{ return -1; }
static off_t fifo_seek (input_plugin_t *this_gen, off_t offset, int origin) 
{ return offset; }
static int   fifo_get_optional_data (input_plugin_t *this_gen, void *data, int data_type) 
{ return INPUT_OPTIONAL_UNSUPPORTED; }
static char* fifo_get_mrl (input_plugin_t *this_gen)
{ return "xvdr:slave:"; }

static off_t fifo_read (input_plugin_t *this_gen, char *buf, off_t len) 
{
  int got = 0;
  LOGERR("fifo_input_plugin::fifo_read() not implemented !");
  exit(-1); /* assert(false); */
  return got;
}

static buf_element_t *fifo_read_block (input_plugin_t *this_gen,
				       fifo_buffer_t *fifo, off_t todo) 
{
  fifo_input_plugin_t *this = (fifo_input_plugin_t *) this_gen;
  LOGDBG("fifo_read_block");

  while(!this->stream->demux_action_pending) {
    buf_element_t *buf = fifo_buffer_try_get(this->buffer);
    if(buf) {
      /* LOGDBG("fifo_read_block: got, return"); */
      return buf;
    }
    /* LOGDBG("fifo_read_block: no buf, poll..."); */
    /* poll(NULL, 0, 10); */
    xine_usec_sleep(5*1000);
    /* LOGDBG("fifo_read_block: poll timeout"); */
  }

  LOGDBG("fifo_read_block: return NULL !");
  return NULL;
}

static void fifo_dispose (input_plugin_t *this_gen) 
{
  fifo_input_plugin_t *this = (fifo_input_plugin_t *) this_gen;
  LOGDBG("fifo_dispose");

  if(this) { 
    if(this->buffer) 
      this->buffer->dispose(this->buffer); 
    free(this); 
  } 
}

static input_plugin_t *fifo_class_get_instance (input_class_t *cls_gen,
						xine_stream_t *stream,
						const char *data) 
{
  fifo_input_plugin_t *slave = (fifo_input_plugin_t *) xine_xmalloc (sizeof(fifo_input_plugin_t));
  unsigned long int imaster;
  vdr_input_plugin_t *master;
  LOGDBG("fifo_class_get_instance");

  sscanf(data+4+1+5+1, "%lx", &imaster);
  master = (vdr_input_plugin_t*)imaster;

  memset(slave, 0, sizeof(fifo_input_plugin_t));
  slave->master = (vdr_input_plugin_t*)master;
  slave->stream = stream;
  slave->buffer_pool = stream->video_fifo;
  slave->buffer = _x_fifo_buffer_new(4,4096);
  slave->i.open              = fifo_open;
  slave->i.get_mrl           = fifo_get_mrl;
  slave->i.dispose           = fifo_dispose;
  slave->i.input_class       = cls_gen;
  slave->i.get_capabilities  = fifo_get_capabilities;
  slave->i.read              = fifo_read;
  slave->i.read_block        = fifo_read_block;
  slave->i.seek              = fifo_seek;
  slave->i.get_current_pos   = fifo_get_current_pos;
  slave->i.get_length        = fifo_get_length;
  slave->i.get_blocksize     = fifo_get_blocksize;
  slave->i.get_optional_data = fifo_get_optional_data;

  return (input_plugin_t*)slave;
}


/******************************** OSD ************************************/

static int update_video_size(vdr_input_plugin_t *this)
{
#if 0
  int w = 0, h = 0;
  int64_t duration;

  this->stream->video_out->status(this->stream->video_out, 
				  this->stream, &w, &h, &duration);

  if(w>0 && h>0) {
    if(this->video_width  != w ||
       this->video_height != h) {
    
      LOGOSD("update_video_size: new video size (%dx%d->%dx%d)",
	     this->video_width, this->video_height, w, h);
      this->video_width = w;
      this->video_height = h;
      this->video_changed = 1;
      return 1;
    }
  }
#endif
  return 0;
}

static xine_rle_elem_t *uncompress_osd_net(uint8_t *raw, int elems, int datalen)
{
  xine_rle_elem_t *data = (xine_rle_elem_t*)malloc(elems*sizeof(xine_rle_elem_t));
  int i;

  for(i=0; i<elems; i++) {
    if((*raw) & 0x80) {
      data[i].len = ((*(raw++)) & 0x7f) << 8;
      data[i].len |= *(raw++);
    } else
      data[i].len = *(raw++);
    data[i].color = *(raw++);
  }

  return data;
}

/* re-scale compressed RLE image */
static xine_rle_elem_t *scale_rle_image(osd_command_t *osdcmd,
					int new_w, int new_h)
{
  #define FACTORBASE      0x100
  #define FACTOR2PIXEL(f) ((f)>>8)
  #define SCALEX(x) FACTOR2PIXEL(factor_x*(x))
  #define SCALEY(y) FACTOR2PIXEL(factor_y*(y))

  xine_rle_elem_t *old_rle = osdcmd->data;
  int old_w = osdcmd->w, old_h = osdcmd->h;
  int old_y = 0, new_y = 0;
  int factor_x = FACTORBASE*new_w/old_w;
  int factor_y = FACTORBASE*new_h/old_h;

  xine_rle_elem_t *new_rle_start, *new_rle, *tmp;
  int rle_size = 8128;
  int num_rle = 0;

  new_rle_start = new_rle = (xine_rle_elem_t*)malloc(4*rle_size);  

  /* we assume rle elements are breaked at end of line */
  while(old_y < old_h) {
    int elems_current_line = 0;
    int old_x = 0, new_x = 0;

    while(old_x < old_w) {      
      int new_x_end = SCALEX(old_x + old_rle->len);

      if(new_x_end >= new_w) 
	new_x_end = new_w;

      new_rle->len   = new_x_end - new_x;
      new_rle->color = old_rle->color;

      old_x += old_rle->len;
      old_rle++;

      if(new_rle->len > 0) { 
	new_x += new_rle->len;
	new_rle++;

	num_rle++;
	elems_current_line++;
	
	if( (num_rle + 1) >= rle_size ) {
	  rle_size *= 2;
	  new_rle_start = (xine_rle_elem_t*)realloc( new_rle_start, 4*rle_size);
	  new_rle = new_rle_start + num_rle;
	}
      }
    }
    if(new_x < new_w)
      (new_rle-1)->len += new_w - new_x;
    old_y++; 
    new_y++;

    if(factor_y > FACTORBASE) {
      /* scale up -- duplicate current line ? */
      int dup = SCALEY(old_y) - new_y;

      /* if no lines left in (old) rle, copy all lines still missing from new */
      if(old_y == old_h) 
	dup = new_h - new_y - 1;

      while(dup-- && (new_y+1<new_h)) {
	xine_rle_elem_t *prevline;
	int n;
	if( (num_rle + elems_current_line + 1) >= rle_size ) {
	  rle_size *= 2;
	  new_rle_start = (xine_rle_elem_t*)realloc( new_rle_start, 4*rle_size);
	  new_rle = new_rle_start + num_rle;
	}
	
	/* duplicate previous line */
	prevline = new_rle - elems_current_line;
	for(n = 0; n < elems_current_line; n++) {
	  *new_rle++ = *prevline++;
	  num_rle++;
	}
	new_y++;
      }

    } else if(factor_y < FACTORBASE) {
      /* scale down -- drop next line ? */
      int skip = new_y - SCALEY(old_y);

      if(old_y == old_h-1) {
	/* one (old) line left ; don't skip it if new rle is not complete */
	if(new_y < new_h)
	  skip = 0;
      }
      while(skip--) {
	for(old_x = 0; old_x < old_w;) {
	  old_x += old_rle->len;
	  old_rle++;
	}
	old_y++;
      }
    }
  }

  tmp = osdcmd->data;

  osdcmd->data = new_rle_start;
  osdcmd->datalen = num_rle*4;

  if(old_w != new_w) {
    osdcmd->x = (0x100*osdcmd->x * new_w/old_w)>>8;
    osdcmd->w = new_w;
  }
  if(old_h != new_h) {
    osdcmd->y = (0x100*osdcmd->y * new_h/old_h)>>8;
    osdcmd->h = new_h;
  }

  return tmp;
}

static int exec_osd_command(vdr_input_plugin_t *this, osd_command_t *cmd)
{
  video_overlay_event_t   ov_event;
  vo_overlay_t            ov_overlay;
  video_overlay_manager_t *ovl_manager;
  int handle = -1, i;

  /* Caller must have locked this->osd_lock ! */

  LOGOSD("exec_osd_command %d", cmd ? cmd->cmd : -1); 

  /* Check parameters */

  if(!cmd || !this || !this->stream) {
    LOGMSG("exec_osd_command: Stream not initialized !");
    return -3;
  }

  if(cmd->wnd < 0 || cmd->wnd >= MAX_OSD_OBJECT) {
    LOGMSG("exec_osd_command: OSD window handle %d out of range !", cmd->wnd);
    return -2;
  }

  handle = this->osdhandle[cmd->wnd];

  if(handle < 0 && cmd->cmd == OSD_Close) {
    LOGMSG("exec_osd_command: Attempt to close non-existing OSD (%d) !", cmd->wnd);
    return -2;
  }

  ovl_manager = 
      this->stream->video_out->get_overlay_manager(this->stream->video_out);

  if(!ovl_manager) {
    LOGMSG("exec_osd_command: Stream has no overlay manager !");
    return -3;
  }

  memset(&ov_event, 0, sizeof(ov_event));

  /* calculate exec time */
  if(cmd->pts || cmd->delay_ms) {
    int64_t vpts = xine_get_current_vpts(this->stream);
    if(cmd->pts) {
      ov_event.vpts = cmd->pts + 
          this->stream->metronom->get_option(this->stream->metronom, 
					     METRONOM_VPTS_OFFSET);
    } else {
      if(this->last_changed_vpts[cmd->wnd]) 
	ov_event.vpts = this->last_changed_vpts[cmd->wnd] + cmd->delay_ms*90;
    }
    /* execution time must be in future */
    if(ov_event.vpts < vpts)
      ov_event.vpts = 0;
    /* limit delay to 5 seconds (because of seeks and channel switches ...) */
    if(ov_event.vpts > vpts + 5*90000)
      ov_event.vpts = vpts + 5*90000;
  }

  /* Execute command */

  if(cmd->cmd == OSD_Size) {
    this->vdr_osd_width  = cmd->w;
    this->vdr_osd_height = cmd->h;

  } else if(cmd->cmd == OSD_Nop) {
    this->last_changed_vpts[cmd->wnd] = xine_get_current_vpts(this->stream);

  } else if(cmd->cmd == OSD_SetPalette) {
    /* TODO */
  } else if(cmd->cmd == OSD_Move) {
    /* TODO */
  } else if(cmd->cmd == OSD_Set_YUV) {
    /* TODO */
  } else if(cmd->cmd == OSD_Close) {
    ov_event.event_type = OVERLAY_EVENT_FREE_HANDLE;
    ov_event.object.handle = handle;
    this->osdhandle[cmd->wnd] = -1;
    if(this->osddata[cmd->wnd].data) {
      free(this->osddata[cmd->wnd].data);
      this->osddata[cmd->wnd].data = NULL;
    }
    if(this->osddata[cmd->wnd].palette) {
      free(this->osddata[cmd->wnd].palette);
      this->osddata[cmd->wnd].palette = NULL;
    }

    do {
      int r = ovl_manager->add_event(ovl_manager, (void *)&ov_event);
      if(r<0) {
	LOGDBG("OSD_Close(%d): overlay manager queue full !", cmd->wnd);
	ovl_manager->flush_events(ovl_manager);
	continue;
      }
    } while(0);

    this->last_changed_vpts[cmd->wnd] = 0;

    if((cmd->wnd==0 || this->osdhandle[cmd->wnd-1]<0) &&
       (cmd->wnd==MAX_OSD_OBJECT || this->osdhandle[cmd->wnd+1]<0)) {
      /*LOGMSG("OSD_Close(%d): last, flush ovl manager");*/
      ovl_manager->flush_events(ovl_manager);
    }

  } else if(cmd->cmd == OSD_Set_RLE) {

    int use_unscaled = 0;
    int rle_scaled = 0;
    int semitransparent = 0;
    int xmove = 0, ymove = 0;
    int unscaled_supported = 1;

    if(handle < 0)
      handle = this->osdhandle[cmd->wnd] = 
	ovl_manager->get_handle(ovl_manager,0);

    ov_event.event_type = OVERLAY_EVENT_SHOW;
    ov_event.object.handle = handle;
    ov_event.object.overlay = &ov_overlay;
    memset( ov_event.object.overlay, 0, sizeof(*ov_event.object.overlay) );

#if XINE_VERSION_CODE < 10101  
    ov_event.object.overlay->clip_top    = -1;
    ov_event.object.overlay->clip_bottom = 0;
    ov_event.object.overlay->clip_left   = 0;
    ov_event.object.overlay->clip_right  = 0;
#else
    ov_event.object.overlay->hili_top    = -1;
    ov_event.object.overlay->hili_bottom = 0;
    ov_event.object.overlay->hili_left   = 0;
    ov_event.object.overlay->hili_right  = 0;
#endif

    /* palette must contain YUV values for each color index */
    for(i=0; i<cmd->colors; i++) {
      uint32_t *tmp = (uint32_t*)(cmd->palette + i);
      ov_event.object.overlay->color[i] = *tmp & 0xffffff;
      ov_event.object.overlay->trans[i] = (cmd->palette[i].alpha + 0x7)/0xf;
      if(ov_event.object.overlay->trans[i] > 0 && 
	 ov_event.object.overlay->trans[i] < 0xf)
	semitransparent = 1;
    }

    if(!(this->stream->video_out->get_capabilities(this->stream->video_out) &
	 VO_CAP_UNSCALED_OVERLAY))
      unscaled_supported = 0;
    else if(this->unscaled_osd ||
	    (this->unscaled_osd_opaque && !semitransparent))
      use_unscaled = 1;

    /* store osd for later rescaling (done if video size changes) */
    if(this->osddata[cmd->wnd].data) {
      free(this->osddata[cmd->wnd].data);
      this->osddata[cmd->wnd].data = NULL;
    }
    if(this->osddata[cmd->wnd].palette) {
      free(this->osddata[cmd->wnd].palette);
      this->osddata[cmd->wnd].palette = NULL;
    }
    memcpy(&this->osddata[cmd->wnd], cmd, sizeof(osd_command_t));
    this->osddata[cmd->wnd].data = NULL;
    if(cmd->palette) {
      this->osddata[cmd->wnd].palette = malloc(4*cmd->colors);
      memcpy(this->osddata[cmd->wnd].palette, cmd->palette, 4*cmd->colors);
    }

    /* if video size differs from expected (VDR osd is designed for 720x576),
       scale osd to video size or use unscaled (display resolution)
       blending */
    if(!use_unscaled) {
      int w_hi = this->vdr_osd_width  * 1100 / 1000;
      int w_lo = this->vdr_osd_width  *  950 / 1000;
      int h_hi = this->vdr_osd_height * 1100 / 1000;
      int h_lo = this->vdr_osd_height *  950 / 1000;
      int width_diff = 0, height_diff = 0;
#if 0
      int video_changed = update_video_size(this);
      LOGOSD("video size %dx%d, margins %d..%dx%d..%d (changed: %s)", 
	     this->video_width, this->video_height, w_lo, w_hi, h_lo, h_hi
	     video_changed ? "yes" : "no");
#endif
      if(this->video_width  < w_lo)  width_diff  = -1;
      else if(this->video_width  > w_hi)  width_diff  =  1;
      if(this->video_height < h_lo) height_diff = -1;
      else if(this->video_height > h_hi) height_diff =  1;

      if(width_diff || height_diff) {
	int new_w = (0x100*cmd->w * this->video_width 
		     / this->vdr_osd_width)>>8;
	int new_h = (0x100*cmd->h * this->video_height 
		     / this->vdr_osd_height)>>8;
	LOGOSD("Size out of margins, rescaling rle image");
	if(width_diff < 0 || height_diff < 0)
	  if(unscaled_supported && this->unscaled_osd_lowresvideo)
	    use_unscaled = 1;

	if(this->rescale_osd) {

	  if(!this->rescale_osd_downscale) {
	    if(width_diff<0) {
	      width_diff = 0;
	      new_w = cmd->w;
	    }
	    if(height_diff<0) {
	      height_diff = 0;
	      new_h = cmd->h;
	    }
	  }
	  if(height_diff || width_diff) {
	    this->osddata[cmd->wnd].data = cmd->data;
	    this->osddata[cmd->wnd].datalen = cmd->datalen;
	    
	    rle_scaled = 1;
	    scale_rle_image(cmd, new_w, new_h);
	  } else {
	    LOGOSD("osd_command: size out of margins, using UNSCALED");
	    use_unscaled = unscaled_supported;
	  }
	}
      }
      if(!use_unscaled && !rle_scaled) {
	/* no scaling required, but may still need to re-center OSD */
	if(this->video_width != this->vdr_osd_width)
	  xmove = (this->video_width - this->vdr_osd_width)/2;
	if(this->video_height != this->vdr_osd_height)
	  ymove = (this->video_height - this->vdr_osd_height)/2;
      }
    }

    if(use_unscaled) {
      int win_width  = this->stream->video_out->get_property(this->stream->video_out, 
							     VO_PROP_WINDOW_WIDTH);
      int win_height = this->stream->video_out->get_property(this->stream->video_out, 
							     VO_PROP_WINDOW_HEIGHT);
      if(this->rescale_osd) {
	/* it is not nice to have subs in _middle_ of display when using 1440x900 etc... */
	
	if(win_width > 240 && win_height > 196) {
	  if(this->rescale_osd) {
	    if(win_width != this->vdr_osd_width || win_height != this->vdr_osd_height) {

	      int new_w = (0x100*cmd->w * win_width 
			   / this->vdr_osd_width)>>8;
	      int new_h = (0x100*cmd->h * win_height 
			   / this->vdr_osd_height)>>8;

	      this->osddata[cmd->wnd].data = cmd->data;
	      this->osddata[cmd->wnd].datalen = cmd->datalen;
	    
	      rle_scaled = 1;
	      scale_rle_image(cmd, new_w, new_h);
	    }
	  }
	}
      }
      if(!rle_scaled) {
	/* no scaling required, but may still need to re-center OSD */
	if(win_width != this->vdr_osd_width)
	  xmove = (win_width - this->vdr_osd_width)/2;
	if(win_height != this->vdr_osd_height)
	  ymove = (win_height - this->vdr_osd_height)/2;
      }
    }
  
    /* set position and size for this overlay */
    ov_event.object.overlay->x = cmd->x + xmove;
    ov_event.object.overlay->y = cmd->y + ymove;
    ov_event.object.overlay->width = cmd->w;
    ov_event.object.overlay->height = cmd->h;

    /* RLE image */
    ov_event.object.overlay->unscaled = use_unscaled;
    ov_event.object.overlay->rle = (rle_elem_t*)cmd->data;
    ov_event.object.overlay->num_rle = cmd->datalen/4; /* two uint_16's in one element */
    ov_event.object.overlay->data_size = cmd->datalen;

    /* store rle for later scaling (done if video size changes) */
    if(/*!use_unscaled &&*/ /*this->rescale_osd && */
       /*!this->osddata[cmd->wnd].data && */
       !rle_scaled /*if scaled, we already have a copy (original data)*/ ) {
      this->osddata[cmd->wnd].data = malloc(cmd->datalen);
      memcpy(this->osddata[cmd->wnd].data, cmd->data, cmd->datalen);
    }
    cmd->data = NULL;/* we 'consume' data (ownership goes for osd manager) */

    /* send event to overlay manager */
    do {
      int r = ovl_manager->add_event(ovl_manager, (void *)&ov_event);
      if(r<0) {
	LOGDBG("OSD_Set_RLE(%d): overlay manager queue full !", cmd->wnd);
	ovl_manager->flush_events(ovl_manager);
	continue;
      }
    } while(0);

    this->last_changed_vpts[cmd->wnd] =  xine_get_current_vpts(this->stream);

  } else {
    LOGMSG("Unknown OSD command %d", cmd->cmd);
    return -2;
  }

  LOGOSD("OSD command %d done", cmd->cmd); 
  return 0;
}

static void vdr_scale_osds(vdr_input_plugin_t *this, 
			   int video_width, int video_height)
{  
  if(! pthread_mutex_lock(&this->osd_lock)) {

    if((this->video_width  != video_width ||
	this->video_height != video_height ||
	this->video_changed) &&
       video_width > 0 && video_height > 0) {
      int i, ticket = 0;

      LOGOSD("New video size (%dx%d->%dx%d)",
	     this->video_width, this->video_height, 
	     video_width, video_height);

      this->video_width = video_width;
      this->video_height = video_height;
      this->video_changed = 0;

      /* just call exec_osd_command for all stored osd's.
         scaling is done automatically if required. */
      for(i=0; i<MAX_OSD_OBJECT; i++)
	if(this->osdhandle[i] >= 0 &&
	   this->osddata[i].data) {
	  osd_command_t tmp;
	  memcpy(&tmp, &this->osddata[i], sizeof(osd_command_t));
	  memset(&this->osddata[i], 0, sizeof(osd_command_t));

	  if(!ticket) {
	    this->stream->xine->port_ticket->acquire(this->stream->xine->port_ticket, 1);
	    ticket++;
	  }
	  exec_osd_command(this, &tmp);

	  if(tmp.palette)
	    free(tmp.palette);
	  if(tmp.data)
	    free(tmp.data);
	}
      if(ticket)
	this->stream->xine->port_ticket->release(this->stream->xine->port_ticket, 1);
    }
    pthread_mutex_unlock(&this->osd_lock);

  } else {
    LOGERR("vdr_scale_osds: pthread_mutex_lock failed");
  }
}

static int vdr_plugin_exec_osd_command(input_plugin_t *this_gen, 
				       osd_command_t *cmd)
{
  vdr_input_plugin_t *this = (vdr_input_plugin_t *) this_gen;
  int result = -3;
  int video_changed = 0;

  if(!pthread_mutex_lock (&this->osd_lock)) {
    video_changed = update_video_size(this);
    this->stream->xine->port_ticket->acquire(this->stream->xine->port_ticket, 1);
    result = exec_osd_command(this, cmd);
    this->stream->xine->port_ticket->release(this->stream->xine->port_ticket, 1);	  
    pthread_mutex_unlock (&this->osd_lock);
  } else {
    LOGERR("vdr_plugin_exec_osd_command: pthread_mutex_lock failed");
  }

  if(video_changed)
    vdr_scale_osds(this, this->video_width, this->video_height);

  return result;
}


/******************************* Control *********************************/

static void suspend_demuxer(vdr_input_plugin_t *this)
{
  this->stream->demux_action_pending = 1;
  signal_buffer_not_empty(this);
  pthread_mutex_lock( &this->stream->demux_lock );
  /* must be paired with resume_demuxer !!! */
}

static void resume_demuxer(vdr_input_plugin_t *this)
{
  /* must be paired with suspend_demuxer !!! */
  this->stream->demux_action_pending = 0;
  pthread_mutex_unlock( &this->stream->demux_lock );
}

static void vdr_x_demux_flush_engine (xine_stream_t *stream, vdr_input_plugin_t *this) 
{
  buf_element_t *buf;

  if(this->curpos > this->discard_index) {
#if 0
    LOGMSG("Possibly flushing too much !!! (diff=%"PRIu64" bytes, "
	   "guard @%" PRIu64 ")", 
	   this->curpos - this->discard_index, this->guard_index);
#endif
    if(this->curpos < this->guard_index) {
      LOGMSG("Guard > current position, decoder flush skipped");
      return;
    }
  }

  stream->xine->port_ticket->acquire(stream->xine->port_ticket, 1);

  if (stream->video_out) {
    stream->video_out->set_property(stream->video_out, VO_PROP_DISCARD_FRAMES, 1);
  }
  if (stream->audio_out) {
    stream->audio_out->set_property(stream->audio_out, AO_PROP_DISCARD_BUFFERS, 1);
  }

  fifo_buffer_clear(stream->video_fifo);
  fifo_buffer_clear(stream->audio_fifo);

  buf = stream->video_fifo->buffer_pool_alloc (stream->video_fifo);
  buf->type = BUF_CONTROL_RESET_DECODER;
  stream->video_fifo->put (stream->video_fifo, buf);

  buf = stream->audio_fifo->buffer_pool_alloc (stream->audio_fifo);
  buf->type = BUF_CONTROL_RESET_DECODER;
  stream->audio_fifo->put (stream->audio_fifo, buf);

  this->prev_audio_stream_id = 0;

  /* on seeking we must wait decoder fifos to process before doing flush.
   * otherwise we flush too early (before the old data has left decoders)
   */
  _x_demux_control_headers_done (stream);

  if (stream->video_out) {
    stream->video_out->flush(stream->video_out);
    stream->video_out->set_property(stream->video_out, 
				    VO_PROP_DISCARD_FRAMES, 0);
  }

  if (stream->audio_out) {
    stream->audio_out->flush(stream->audio_out);
    stream->audio_out->set_property(stream->audio_out, 
				    AO_PROP_DISCARD_BUFFERS, 0);
  }

  stream->xine->port_ticket->release(stream->xine->port_ticket, 1);
}

static void vdr_x_demux_control_newpts( xine_stream_t *stream, int64_t pts, 
					uint32_t flags ) {

  buf_element_t *buf;

  buf = stream->audio_fifo->buffer_pool_try_alloc (stream->audio_fifo);
  if(buf) {
    buf->type = BUF_CONTROL_NEWPTS;
    buf->decoder_flags = flags;
    buf->disc_off = pts;
    stream->video_fifo->put (stream->video_fifo, buf); 
 } else {
    LOGMSG("vdr_x_demux_control_newpts BUFFER FULL!");
  }
  if(buf) {
    buf = stream->audio_fifo->buffer_pool_try_alloc (stream->audio_fifo);
    buf->type = BUF_CONTROL_NEWPTS;
    buf->decoder_flags = flags;
    buf->disc_off = pts;
    stream->audio_fifo->put (stream->audio_fifo, buf);
  } else {
    LOGMSG("vdr_x_demux_control_newpts BUFFER FULL!");
  }
}

static void vdr_flush_engine(vdr_input_plugin_t *this)
{
  if(!this->stream_start) {
    /* suspend demuxer */
    pthread_mutex_unlock( &this->lock ); /* to let demuxer return from vdr_plugin_read_* */
    suspend_demuxer(this);
    pthread_mutex_lock( &this->lock );

    reset_scr_tunning(this, this->speed_before_pause);

    vdr_x_demux_flush_engine (this->stream, this);

#if 0
    /* disabled _x_demux_control_start as it causes alsa output driver to exit now and then ...*/
    _x_demux_control_start(this->stream);
#endif
    this->stream_start = 1;

    resume_demuxer(this);
  } else {
    /*LOGMSG("vdr_flush_engine: stream_start=true, skipped flush");*/
  }
}

static int set_deinterlace_method(vdr_input_plugin_t *this, const char *method_name)
{
  int method = 0;
  if(!strncasecmp(method_name,"bob",3)) {                 method = 1;
  } else if(!strncasecmp(method_name,"weave",5)) {        method = 2;
  } else if(!strncasecmp(method_name,"greedy",6)) {       method = 3;
  } else if(!strncasecmp(method_name,"onefield",8)) {     method = 4;
  } else if(!strncasecmp(method_name,"onefield_xv",11)) { method = 5;
  } else if(!strncasecmp(method_name,"linearblend",11)) { method = 6;
  } else if(!strncasecmp(method_name,"none",4)) {         method = 0;
  } else if(!*method_name) {                          method = 0;
  } else if(!strncasecmp(method_name,"tvtime",6)) {       method = 0; 
  /* old deinterlacing system must be switched off.
     tvtime will be configured as all other post plugins with 
     "POST tvtime ..." control message  */
  } else return -2;
  
  this->stream->xine->config->update_num(this->stream->xine->config,
					 "video.output.xv_deinterlace_method",
					 method);
  xine_set_param(this->stream, XINE_PARAM_VO_DEINTERLACE, method ? 1 : 0);
  
  return 0;
}

static int set_video_properties(vdr_input_plugin_t *this, 
				int hue, int saturation, 
				int brightness, int contrast)
{
  pthread_mutex_lock(&this->lock);

  /* when changed first time, save original/default values */
  if(!this->video_properties_saved && 
     (hue>=0 || saturation>=0 || contrast>=0 || brightness>=0)) {
    this->video_properties_saved = 1;
    this->orig_hue        = xine_get_param(this->stream, 
					   XINE_PARAM_VO_HUE );
    this->orig_saturation = xine_get_param(this->stream, 
					   XINE_PARAM_VO_SATURATION );
    this->orig_brightness = xine_get_param(this->stream, 
					   XINE_PARAM_VO_BRIGHTNESS );
    this->orig_contrast   = xine_get_param(this->stream, 
					   XINE_PARAM_VO_CONTRAST );
  }

  /* set new values, or restore default/original values */
  if(hue>=0 || this->video_properties_saved)
    xine_set_param(this->stream, XINE_PARAM_VO_HUE, 
		   hue>=0 ? hue : this->orig_hue );
  if(saturation>=0 || this->video_properties_saved)
    xine_set_param(this->stream, XINE_PARAM_VO_SATURATION, 
		   saturation>0 ? saturation : this->orig_saturation );
  if(brightness>=0 || this->video_properties_saved)
    xine_set_param(this->stream, XINE_PARAM_VO_BRIGHTNESS, 
		   brightness>=0 ? brightness : this->orig_brightness );
  if(contrast>=0 || this->video_properties_saved)
    xine_set_param(this->stream, XINE_PARAM_VO_CONTRAST, 
		   contrast>=0 ? contrast : this->orig_contrast );

  if(hue<0 && saturation<0 && contrast<0 && brightness<0)
    this->video_properties_saved = 0;

  pthread_mutex_unlock(&this->lock);
  return 0;
}

static int set_live_mode(vdr_input_plugin_t *this, int onoff)
{
  pthread_mutex_lock(&this->lock);

  if(this->live_mode != onoff) {
    config_values_t *config = this->stream->xine->config;
    this->live_mode = onoff;

    this->stream->metronom->set_option(this->stream->metronom, 
				       METRONOM_PREBUFFER, METRONOM_PREBUFFER_VAL);

    if(!this->live_mode || (this->fd_control > 0 && !this->slave_stream)) {
      config->update_num(this->stream->xine->config,
			 "audio.synchronization.av_sync_method", 1);
    } else {
      config->update_num(this->stream->xine->config,
			 "audio.synchronization.av_sync_method", 0);
    }
  }

  /* set buffer usage limits */
  this->max_buffers = this->buffer_pool->buffer_pool_capacity;
  if(this->live_mode && this->fd_control < 0) 
    this->max_buffers >>= 1;
  this->max_buffers -= 10;

  if(this->no_video) 
    this->max_buffers = RADIO_MAX_BUFFERS;

  /* SCR tunning */
  if(this->live_mode) {
    LOGSCR("pause scr tunning by set_live_mode");
    scr_tunning_set_paused(this);
  } else {
    LOGSCR("reset scr tunning by set_live_mode");
    reset_scr_tunning(this, this->speed_before_pause=XINE_FINE_SPEED_NORMAL);
  }

  this->still_mode = 0;

  pthread_mutex_unlock(&this->lock);

  signal_buffer_pool_not_empty(this);
  return 0;
}

static int  set_playback_speed(vdr_input_plugin_t *this, int speed)
{
/*  speed:
      <0 - show each abs(n)'th frame (drop other frames)
        * no audio
      0 - paused
        * audio back if mute != 0
      >0 - show each frame n times
        * no audio
      1 - normal
*/
  pthread_mutex_lock(&this->lock);
  this->is_paused = 0;
  if(speed == 0) {
    this->is_paused = 1;
  } else if(speed>64 || speed<-64) {
    pthread_mutex_unlock(&this->lock);
    return -2;
  }

  if(speed>0)
    speed = this->speed_before_pause = XINE_FINE_SPEED_NORMAL/speed;
  else
    speed = this->speed_before_pause = XINE_FINE_SPEED_NORMAL*(-speed);

  if(this->scr_tunning != SCR_TUNNING_PAUSED && 
     _x_get_fine_speed(this->stream) != speed) {
    _x_set_fine_speed (this->stream, speed);
  }

  if(this->slave_stream) 
    _x_set_fine_speed (this->slave_stream, speed);

  pthread_mutex_unlock(&this->lock);
  return 0;
}

static void vdr_event_cb (void *user_data, const xine_event_t *event);

static int handle_control_playfile(vdr_input_plugin_t *this, const char *cmd)
{
  const char *pt = cmd + 9;
  char filename[1024]="", *subs = NULL;
  int loop = 0, pos = 0, err = 0;

  while(*pt==' ') pt++;

  if(!strncmp(pt, "Loop ", 5)) {
    loop = 1;
    pt += 5;
    while(*pt==' ') pt++;
  }

  pos = atoi(pt);

  while(*pt && *pt != ' ') pt++;
  while(*pt == ' ') pt++;

  strncpy(filename, pt, 1023);
  filename[1023] = 0;

  LOGMSG("PLAYFILE  (Loop: %d, Offset: %ds, File: %s)",
	 loop, pos, *filename ? filename : "<STOP>" );

  if(*filename) {
    this->loop_play = 0;
    if(this->slave_stream) {
      handle_control_playfile(this, "PLAYFILE 0");
    }
    subs = FindSubFile(filename);
    if(subs) {
      LOGMSG("Found subtitles: %s", subs);
      strcat(filename, "#subtitle:");
      strcat(filename, subs);
      free(subs);
    } else {
      LOGDBG("Subtitles not found for %s", filename);
    }
    this->slave_stream = xine_stream_new(this->stream->xine, 
					 this->stream->audio_out, 
					 this->stream->video_out);

    this->slave_event_queue = xine_event_new_queue (this->slave_stream);
    xine_event_create_listener_thread (this->slave_event_queue, 
				       vdr_event_cb, this);

    err = !xine_open(this->slave_stream, filename);
    if(!err) {
      this->loop_play = loop;
      err = !xine_play(this->slave_stream, 0, 1000 * pos);
    }
    if(!err) {
      set_playback_speed(this, 1);
      this->live_mode = 1;
      set_live_mode(this, 0);
      set_playback_speed(this, 1);
      reset_scr_tunning(this, this->speed_before_pause = XINE_FINE_SPEED_NORMAL);
      this->stream->metronom->set_option(this->stream->metronom, 
					 METRONOM_PREBUFFER, 90000);

      if(this->funcs.fe_control) {
	char tmp[128];
	sprintf(tmp, "SLAVE 0x%lx\r\n", (unsigned long int)this->slave_stream);
	this->funcs.fe_control(this->funcs.fe_handle, tmp);
      }
    } else {
      LOGMSG("Error playing file ! (File not found ? Unknown format ?)");
      *filename = 0;
    }
  }

  if(!*filename) {
    LOGMSG("PLAYFILE <STOP>: Closing slave stream");
    this->loop_play = 0;
    if(this->slave_stream) {
      if (this->slave_event_queue) {
	xine_event_dispose_queue (this->slave_event_queue);
	this->slave_event_queue = NULL;
      }
      if(this->funcs.fe_control) 
	this->funcs.fe_control(this->funcs.fe_handle, "SLAVE 0x0\r\n");
      if(this->fd_control>=0)
	write_control(this, "ENDOFSTREAM\r\n");
      xine_stop(this->slave_stream);
      xine_close(this->slave_stream);
      xine_dispose(this->slave_stream);
      this->slave_stream = NULL;
    }
  }

  return err ? CONTROL_PARAM_ERROR : CONTROL_OK;
}

static int handle_control_substream(vdr_input_plugin_t *this, const char *cmd)
{
  unsigned int pid;
  if(1 == sscanf(cmd, "SUBSTREAM 0x%x", &pid)) {
    pthread_mutex_lock(&this->lock);
    
    if(!this->funcs.fe_control)
      LOGERR("ERROR - no fe_control set !");
    
    if((pid & 0xf0) == 0xe0 && this->funcs.fe_control) { /* video 0...15 */
      if(!this->pip_stream) {
LOGMSG("create pip stream %s", cmd);
        this->pip_stream = 
	  this->funcs.fe_control(this->funcs.fe_handle, cmd);
LOGMSG("  pip stream created");
      }
    } else {
  /*} else if(audio) {*/
      if(this->pip_stream && this->funcs.fe_control) {
	LOGMSG("close pip stream");
 
	this->pip_stream = NULL;
	this->funcs.fe_control(this->funcs.fe_handle, cmd);
	/* xine_stop(this->pip_stream); */
	/* xine_close(this->pip_stream); */
	/* xine_dispose(this->pip_stream); */
      }
    }
    pthread_mutex_unlock(&this->lock);
    return CONTROL_OK;
  } 
  return CONTROL_PARAM_ERROR;
}

static int handle_control_osdscaling(vdr_input_plugin_t *this, const char *cmd)
{
  int err = CONTROL_OK;
  pthread_mutex_lock(&this->osd_lock);
  if(1 == sscanf(cmd, "OSDSCALING %d", &this->rescale_osd)) {
    this->rescale_osd_downscale = strstr(cmd, "NoDownscale") ? 0 : 1;
    this->unscaled_osd = strstr(cmd, "UnscaledAlways") ? 1 : 0;
    this->unscaled_osd_opaque = strstr(cmd, "UnscaledOpaque") ? 1 : 0;
    this->unscaled_osd_lowresvideo = strstr(cmd, "UnscaledLowRes") ? 1 : 0;
  } else
    err = CONTROL_PARAM_ERROR;
  pthread_mutex_unlock(&this->osd_lock);
  return err;
}

static int handle_control_osdcmd(vdr_input_plugin_t *this)
{
  osd_command_t osdcmd;
  int err = CONTROL_OK;

  if(this->fd_control < 0)
    return CONTROL_DISCONNECTED;

  if(read_control(this, (unsigned char*)&osdcmd, sizeof(osd_command_t))
     != sizeof(osd_command_t)) {
    LOGMSG("control: error reading OSDCMD data");
    return CONTROL_DISCONNECTED;
  }

#if __BYTE_ORDER == __LITTLE_ENDIAN
  /* -> host order */
  osdcmd.cmd = ntohl(osdcmd.cmd);
  osdcmd.wnd = ntohl(osdcmd.wnd);
  osdcmd.pts = ntohll(osdcmd.pts);
  osdcmd.delay_ms = ntohl(osdcmd.delay_ms);
  osdcmd.x = ntohs(osdcmd.x);
  osdcmd.y = ntohs(osdcmd.y);
  osdcmd.w = ntohs(osdcmd.w);
  osdcmd.h = ntohs(osdcmd.h);
  osdcmd.datalen = ntohl(osdcmd.datalen);
  osdcmd.num_rle = ntohl(osdcmd.num_rle);
  osdcmd.colors  = ntohl(osdcmd.colors);
#elif __BYTE_ORDER == __BIG_ENDIAN
#else
#  error __BYTE_ORDER undefined !
#endif

  if(osdcmd.palette && osdcmd.colors>0) {
    int bytes = sizeof(xine_clut_t)*(osdcmd.colors);
    osdcmd.palette = malloc(bytes);
    if(read_control(this, (unsigned char *)osdcmd.palette, bytes)
       != bytes) {
      LOGMSG("control: error reading OSDCMD palette");
      err = CONTROL_DISCONNECTED;
    }
  } else {
    osdcmd.palette = NULL;
  }

  if(err == CONTROL_OK && osdcmd.data && osdcmd.datalen>0) {
    osdcmd.data = (xine_rle_elem_t*)malloc(osdcmd.datalen);
    if(read_control(this, (unsigned char *)osdcmd.data, osdcmd.datalen)
       != osdcmd.datalen) {
      LOGMSG("control: error reading OSDCMD bitmap");
      err = CONTROL_DISCONNECTED;
    } else {
      uint8_t *raw = osdcmd.raw_data;
      osdcmd.data = uncompress_osd_net(raw, osdcmd.num_rle, osdcmd.datalen);
      osdcmd.datalen = osdcmd.num_rle*4;
      free(raw);
    }
  } else {
    osdcmd.data = NULL;
  }

  if(err == CONTROL_OK) 
    err = vdr_plugin_exec_osd_command((input_plugin_t*)this, &osdcmd);

  if(osdcmd.data)
    free(osdcmd.data);
  if(osdcmd.palette)
    free(osdcmd.palette);

  return err;
}

/************************** Control from VDR ******************************/

#define VDR_ENTRY_LOCK(ret...) \
  do { \
    if(pthread_mutex_lock(&this->vdr_entry_lock)) { \
      LOGERR("%s:%d: pthread_mutex_lock failed", __PRETTY_FUNCTION__, __LINE__); \
      return ret ; \
    } \
  } while(0)

#define VDR_ENTRY_UNLOCK() \
  do { \
    if(pthread_mutex_unlock(&this->vdr_entry_lock)) { \
      LOGERR("%s:%d: pthread_mutex_unlock failed", __PRETTY_FUNCTION__, __LINE__); \
    } \
  } while(0)

static const struct { int type; char *name; } eventmap[] = {
  {XINE_EVENT_INPUT_UP,      "XINE_EVENT_INPUT_UP"},
  {XINE_EVENT_INPUT_DOWN,    "XINE_EVENT_INPUT_DOWN"},
  {XINE_EVENT_INPUT_LEFT,    "XINE_EVENT_INPUT_LEFT"},
  {XINE_EVENT_INPUT_RIGHT,   "XINE_EVENT_INPUT_RIGHT"},
  {XINE_EVENT_INPUT_SELECT,  "XINE_EVENT_INPUT_SELECT"},
  {XINE_EVENT_INPUT_MENU1,   "XINE_EVENT_INPUT_MENU1"},
  {XINE_EVENT_INPUT_MENU2,   "XINE_EVENT_INPUT_MENU2"},
  {XINE_EVENT_INPUT_MENU3,   "XINE_EVENT_INPUT_MENU3"},
  {XINE_EVENT_INPUT_MENU4,   "XINE_EVENT_INPUT_MENU4"},
  {XINE_EVENT_INPUT_MENU5,   "XINE_EVENT_INPUT_MENU5"},
  {XINE_EVENT_INPUT_NEXT,    "XINE_EVENT_INPUT_NEXT"},
  {XINE_EVENT_INPUT_PREVIOUS,"XINE_EVENT_INPUT_PREVIOUS"},
  {-1, NULL}};

static int vdr_plugin_poll(vdr_input_plugin_t *this, int timeout_ms)
{
  struct timespec abstime;
  int result = 0;

  /* Caller must have locked this->vdr_entry_lock ! */

  if(this->slave_stream) {
    LOGMSG("vdr_plugin_poll: called while playing slave stream !");
    return 1;
  }

  TRACE("vdr_plugin_poll (%d ms), buffer_pool: blocks=%d, bytes=%d", 
	timeout_ms, this->buffer_pool->size(this->buffer_pool), 
	this->buffer_pool->data_size(this->buffer_pool));
#if 0
  if(this->is_paused) {
    VDR_ENTRY_UNLOCK();

    create_timeout_time(&abstime, timeout_ms);
    pthread_mutex_lock (&this->buffer_pool->buffer_pool_mutex);
    while(pthread_cond_timedwait (&this->buffer_pool->buffer_pool_cond_not_empty,
				  &this->buffer_pool->buffer_pool_mutex, 
				  &abstime) != ETIMEDOUT)
      ;
    pthread_mutex_unlock (&this->buffer_pool->buffer_pool_mutex);
    timeout_ms = 0;

    VDR_ENTRY_LOCK(0);
  }
#endif
  pthread_mutex_lock (&this->buffer_pool->buffer_pool_mutex);
  result = this->buffer_pool->buffer_pool_num_free - 
           (this->buffer_pool->buffer_pool_capacity - this->max_buffers);
  pthread_mutex_unlock (&this->buffer_pool->buffer_pool_mutex);

  if(timeout_ms > 0 && result <= 0) {
    create_timeout_time(&abstime, timeout_ms);

    pthread_mutex_lock(&this->lock);
    if(this->scr_tunning == SCR_TUNNING_PAUSED) {
      LOGSCR("scr tunning reset by POLL");
      reset_scr_tunning(this,this->speed_before_pause);
    }
    pthread_mutex_unlock(&this->lock);

    signal_buffer_not_empty(this);

    VDR_ENTRY_UNLOCK();

    pthread_mutex_lock (&this->buffer_pool->buffer_pool_mutex);
    while(result <= 5) {
      if(pthread_cond_timedwait (&this->buffer_pool->buffer_pool_cond_not_empty,
				 &this->buffer_pool->buffer_pool_mutex, 
				 &abstime) == ETIMEDOUT)
	break;      
      result = this->buffer_pool->buffer_pool_num_free - 
	       (this->buffer_pool->buffer_pool_capacity - this->max_buffers);
    }
    pthread_mutex_unlock (&this->buffer_pool->buffer_pool_mutex);

    VDR_ENTRY_LOCK(0);
  }

  TRACE("vdr_plugin_poll returns, %d free (%d used, %d bytes)\n", result, 
	this->buffer_pool->size(this->buffer_pool), 
	this->buffer_pool->data_size(this->buffer_pool));

 /* handle priority problem in paused mode when 
    data source has higher priority than control source */
  if(result <= 0) {
    result = 0;
    pthread_yield();
    xine_usec_sleep(3*1000);
  }

  return result;
}

/*
 * Flush returns 0 if there is no data or frames in stream buffers 
 */
static int vdr_plugin_flush(vdr_input_plugin_t *this, int timeout_ms)
{
  struct timespec abstime;
  fifo_buffer_t *pool   = this->buffer_pool;
  fifo_buffer_t *buffer = this->block_buffer;
  int result = 0, waitresult=0;

  /* Caller must have locked this->vdr_entry_lock ! */

  if(this->slave_stream) {
    LOGDBG("vdr_plugin_flush: called while playing slave stream !");
    return 0;
  }

  TRACE("vdr_plugin_flush (%d ms) blocks=%d+%d, frames=%d", timeout_ms,
	buffer->size(buffer), pool->size(pool),
	this->stream->video_out->get_property(this->stream->video_out, 
					      VO_PROP_BUFS_IN_FIFO));

  if(this->live_mode /*&& this->fd_control < 0*/) {
    /* No flush in live mode */
    sched_yield();
    return 1;
  }

  result = MAX(0, pool->size(pool)) + 
           MAX(0, buffer->size(buffer)) +
           this->stream->video_out->get_property(this->stream->video_out, 
						 VO_PROP_BUFS_IN_FIFO);
  if(result>0) {
    put_control_buf(buffer, pool, BUF_CONTROL_FLUSH_DECODER);
    put_control_buf(buffer, pool, BUF_CONTROL_NOP);
  }

  create_timeout_time(&abstime, timeout_ms);

  while(result > 0 && waitresult != ETIMEDOUT) {
    TRACE("vdr_plugin_flush waiting (max %d ms), %d+%d buffers used, "
	  "%d frames (rd pos=%" PRIu64 ")\n", timeout_ms,
	  pool->size(pool), buffer->size(buffer),
	  (int)this->stream->video_out->get_property(this->stream->video_out, 
						     VO_PROP_BUFS_IN_FIFO),
	  this->curpos);

    pthread_mutex_lock(&pool->buffer_pool_mutex);
    waitresult = pthread_cond_timedwait (&pool->buffer_pool_cond_not_empty, 
					 &pool->buffer_pool_mutex, &abstime);
    pthread_mutex_unlock(&pool->buffer_pool_mutex);
    result = MAX(0, pool->size(pool)) +
             MAX(0, buffer->size(buffer)) +
             this->stream->video_out->get_property(this->stream->video_out, 
						   VO_PROP_BUFS_IN_FIFO);
  }

  TRACE("vdr_plugin_flush returns %d (%d+%d used, %d frames)\n", result,
	pool->size(pool), buffer->size(buffer),
	(int)this->stream->video_out->get_property(this->stream->video_out, 
						   VO_PROP_BUFS_IN_FIFO));
  
  return result;
}

static int vdr_plugin_flush_remote(vdr_input_plugin_t *this, int timeout_ms, 
				   uint64_t offset, int frame)
{
  int r, live_mode;

  pthread_mutex_lock(&this->lock);

  live_mode = this->live_mode;
  this->live_mode = 0; /* --> 1 again when data arrives ... */

  LOGSCR("reset scr tunning by flush_remote");
  reset_scr_tunning(this, this->speed_before_pause);

  /* wait until all data has been received */
  while(this->curpos < offset && timeout_ms > 0) {
    TRACE("FLUSH: wait position (%" PRIu64 " ; need %" PRIu64 ")", 
	  this->curpos, offset);
    pthread_mutex_unlock(&this->lock);
    xine_usec_sleep(3*1000);
    pthread_mutex_lock(&this->lock);
    timeout_ms -= 3;
  }

  LOGSCR("reset scr tunning by flush_remote");
  reset_scr_tunning(this, this->speed_before_pause);

  pthread_mutex_unlock(&this->lock);

  r = vdr_plugin_flush(this, MAX(5, timeout_ms));
  printf_control(this, "RESULT %d %d\r\n", this->token, r);

  pthread_mutex_lock(&this->lock);

  this->live_mode = live_mode;
  this->stream->metronom->set_option(this->stream->metronom, 
				     METRONOM_PREBUFFER, 
				     METRONOM_PREBUFFER_VAL);
  this->guard_index = offset;

  pthread_mutex_unlock(&this->lock);

  return CONTROL_OK;
}

static int vdr_plugin_parse_control(input_plugin_t *this_gen, const char *cmd)
{
  vdr_input_plugin_t *this = (vdr_input_plugin_t *) this_gen;
  int err = CONTROL_OK, i, j;
  int /*int32_t*/ tmp32 = 0;
  uint64_t tmp64 = 0ULL;
  xine_stream_t *stream = this->stream;
  static const char *str_poll = "POLL";
  char *pt;

  VDR_ENTRY_LOCK(CONTROL_DISCONNECTED);

  LOGCMD("vdr_plugin_parse_control: %s", cmd); 

  if( *((uint32_t*)cmd) == *((uint32_t*)str_poll) ||
      !strncasecmp(cmd, "POLL ", 5)) {
    tmp32 = atoi(cmd+5);
    if(tmp32 >= 0 && tmp32 < 1000) {
      if(this->fd_control >= 0) {
	printf_control(this, "POLL %d\r\n", vdr_plugin_poll(this, tmp32));
      } else {
	err = vdr_plugin_poll(this, tmp32);
      }
    } else {
      err = CONTROL_PARAM_ERROR;
    }
    VDR_ENTRY_UNLOCK();
    return err;
  }

  if(this->slave_stream)
    stream = this->slave_stream;

  if(NULL != (pt = strstr(cmd, "\r\n")))
    *((char*)pt) = 0; /* auts */

  if(!strncasecmp(cmd, "OSDSCALING", 10)) {
    err = handle_control_osdscaling(this, cmd);

  } else if(!strncasecmp(cmd, "OSDCMD", 6)) {
    err = handle_control_osdcmd(this);

  } else if(!strncasecmp(cmd, "VIDEO_PROPERTIES ", 17)) {
    int hue, saturation, brightness, contrast;
    if(4 == sscanf(cmd, "VIDEO_PROPERTIES %d %d %d %d", 
		   &hue, &saturation, &brightness, &contrast))
      err = set_video_properties(this, hue, saturation, brightness, contrast);
    else
      err = CONTROL_PARAM_ERROR;

  } else if(!strncasecmp(cmd, "DEINTERLACE ", 12)) {
    if(this->fd_control < 0)
      err = set_deinterlace_method(this, cmd+12);

  } else if(!strncasecmp(cmd, "EVENT ", 6)) {
    int i=0;
    char *pt = strchr(cmd, '\n');
    if(pt) *pt=0;
    while(eventmap[i].name)
      if(!strcmp(cmd+6, eventmap[i].name)) {
	xine_event_t ev;
	ev.type = eventmap[i].type;
	ev.stream = this->slave_stream ? this->slave_stream : this->stream;
	/* tag event to prevent circular input events 
	   (vdr -> here -> event_listener -> vdr -> ...) */
	ev.data = "VDR"; 
	ev.data_length = 4;
	xine_event_send(ev.stream, &ev);
	break;
      } else {
	i++;
      }

  } else if(!strncasecmp(cmd, "NOVIDEO ", 8)) {
    if(1 == sscanf(cmd, "NOVIDEO %d", &tmp32)) {
      pthread_mutex_lock(&this->lock);
      this->no_video = tmp32;
      if(this->no_video) {
        this->max_buffers = RADIO_MAX_BUFFERS;
      } else {
        this->max_buffers = this->buffer_pool->buffer_pool_capacity;
        if(!this->live_mode && this->fd_control < 0) 
	  this->max_buffers >>= 1;
        this->max_buffers -= 10;
      }
      pthread_mutex_unlock(&this->lock);
    } else
      err = CONTROL_PARAM_ERROR;

    signal_buffer_pool_not_empty(this);

  } else if(!strncasecmp(cmd, "DISCARD ", 8)) {
    if(2 == sscanf(cmd, "DISCARD %" PRIu64 " %d", &tmp64, &tmp32)) {
      pthread_mutex_lock(&this->lock);
      this->discard_index = tmp64;
      this->discard_frame = tmp32;
      vdr_flush_engine(this);
      this->I_frames = this->B_frames = this->P_frames = 0;
      pthread_mutex_unlock(&this->lock);
    } else 
      err = CONTROL_PARAM_ERROR;

  } else if(!strncasecmp(cmd, "STREAMPOS ", 10)) {
    if(1 == sscanf(cmd, "STREAMPOS %" PRIu64, &tmp64)) {
      pthread_mutex_lock(&this->lock);
      vdr_flush_engine(this);
      this->curpos = tmp64;
      this->discard_index = this->curpos;
      this->guard_index = 0;
      pthread_mutex_unlock(&this->lock);
    } else 
      err = CONTROL_PARAM_ERROR;

  } else if(!strncasecmp(cmd, "TRICKSPEED ", 11)) {
    err = (1 == sscanf(cmd, "TRICKSPEED %d", &tmp32)) ? 
      set_playback_speed(this, tmp32) : 
      CONTROL_PARAM_ERROR;    

  } else if(!strncasecmp(cmd, "STILL ", 6)) {
    pthread_mutex_lock(&this->lock);
    /*if(this->fd_control >= 0) {*/
      if(1 == sscanf(cmd, "STILL %d", &tmp32)) {
	this->still_mode = tmp32;
	if(this->still_mode)
	  reset_scr_tunning(this, this->speed_before_pause);
	this->stream_start = 1;
      } else
	err = CONTROL_PARAM_ERROR;
    /*}*/
    pthread_mutex_unlock(&this->lock);

  } else if(!strncasecmp(cmd, "LIVE ", 5)) {
      this->still_mode = 0;
      err = (1 == sscanf(cmd, "LIVE %d", &tmp32)) ?
	set_live_mode(this, tmp32) : -2 ;

  } else if(!strncasecmp(cmd, "VOLUME ", 7)) {
    if(1 == sscanf(cmd, "VOLUME %d", &tmp32)) {
      xine_set_param(stream, XINE_PARAM_AUDIO_VOLUME, tmp32);
      xine_set_param(stream, XINE_PARAM_AUDIO_MUTE, tmp32<=0 ? 1 : 0);
    } else
      err = CONTROL_PARAM_ERROR;

  } else if(!strncasecmp(cmd, "AUDIOCOMPRESSION ",17)) {
    if(1 == sscanf(cmd, "AUDIOCOMPRESSION %d", &tmp32)) {
      xine_set_param(stream, XINE_PARAM_AUDIO_COMPR_LEVEL, tmp32);
    } else
      err = CONTROL_PARAM_ERROR;

  } else if(!strncasecmp(cmd, "AUDIOSURROUND ",14)) {
    if(1 == sscanf(cmd, "AUDIOSURROUND %d", &tmp32)) {
      stream->xine->config->update_num(stream->xine->config,
				       "audio.a52.surround_downmix", tmp32?1:0);
    } else
      err = CONTROL_PARAM_ERROR;

  } else if(!strncasecmp(cmd, "EQUALIZER ", 10)) {
    int eqs[XINE_PARAM_EQ_16000HZ - XINE_PARAM_EQ_30HZ + 2] = {0};
    sscanf(cmd,"EQUALIZER %d %d %d %d %d %d %d %d %d %d",
	   eqs,eqs+1,eqs+2,eqs+3,eqs+4,eqs+5,eqs+6,eqs+7,eqs+8,eqs+9);
    for(i=XINE_PARAM_EQ_30HZ,j=0; i<=XINE_PARAM_EQ_16000HZ; i++,j++)
      xine_set_param(stream, i, eqs[j]);

  } else if(!strncasecmp(cmd, "AUDIOSTREAM ", 12)) {
    if(!this->slave_stream) {
      int ac3 = strncmp(cmd+12, "AC3", 3) ? 0 : 1;
      if(1 == sscanf(cmd+12 + 4*ac3, "%d", &tmp32)) {
	pthread_mutex_lock(&this->lock);
	if(this->reset_audio_cnt < 0)
	  this->reset_audio_cnt = 0;
	this->reset_audio_cnt++; 
	this->audio_stream_id = tmp32;
	pthread_mutex_unlock(&this->lock);
      } else {
	err = CONTROL_PARAM_ERROR;
      }
    }

  } else if(!strncasecmp(cmd, "SPUSTREAM ", 10)) {
    if(1 == sscanf(cmd, "SPUSTREAM %d", &tmp32)) {
      buf_element_t *buf_elem = 
	stream->video_fifo->buffer_pool_try_alloc (stream->video_fifo);
      /*if(tmp32<0) LOGMSG("SPU off (-1)");*/
      if(buf_elem) {
	/*LOGDBG("SPU channel selected: %d", tmp32);*/
	tmp32 &= 0x1f;
	buf_elem->type = BUF_CONTROL_SPU_CHANNEL;
	buf_elem->decoder_info[0] = tmp32;  /* widescreen / auto stream id */
	buf_elem->decoder_info[1] = tmp32;  /* letterbox stream id */
	buf_elem->decoder_info[2] = tmp32;  /* pan&scan stream id */
	this->block_buffer->put(this->block_buffer, buf_elem);
      }
    } else 
      err = CONTROL_PARAM_ERROR;

  } else if(!strncasecmp(cmd, "AUDIODELAY ", 11)) {
    if(1 == sscanf(cmd, "AUDIODELAY %d", &tmp32))
      xine_set_param(stream, XINE_PARAM_AV_OFFSET, tmp32*90000/1000);
    else
      err = CONTROL_PARAM_ERROR;

  } else if(!strncasecmp(cmd, "SYNC ", 5)) {
    if(this->fd_control >= 0)
      write_control(this, cmd);

  } else if(!strncasecmp(cmd, "GETSTC", 6)) {
    int64_t pts = xine_get_current_vpts(stream) -
                  stream->metronom->get_option(stream->metronom, 
					       METRONOM_VPTS_OFFSET);
    if(this->fd_control >= 0) {
      printf_control(this, "STC %" PRId64 "\r\n", pts);
    } else {
      *((int64_t *)cmd) = pts;
    }

  } else if(!strncasecmp(cmd, "FLUSH ", 6)) {
    if(1 == sscanf(cmd, "FLUSH %d", &tmp32)) {
      if(this->fd_control >= 0) {
	uint32_t frame = 0;
	tmp64 = 0ULL; 
	tmp32 = 0;
	sscanf(cmd, "FLUSH %d %" PRIu64 " %d", &tmp32, &tmp64, &frame);
	err = vdr_plugin_flush_remote(this, tmp32, tmp64, frame);
      } else {
	err = vdr_plugin_flush(this, tmp32);
      }
    } else
      err = CONTROL_PARAM_ERROR;

  } else if(!strncasecmp(cmd, "TOKEN ", 6)) {
    this->token = atoi(cmd+6);

  } else if(!strncasecmp(cmd, "SUBSTREAM ", 9)) {
    err = handle_control_substream(this, cmd);

  } else if(!strncasecmp(cmd, "POST ", 5)) {
    if(!this->funcs.fe_control)
      LOGMSG("No fe_control function! %s failed.", cmd);
    else
      this->funcs.fe_control(this->funcs.fe_handle, cmd);

  } else if(!strncasecmp(cmd, "PLAYFILE ", 9)) {
    err = handle_control_playfile(this, cmd);
    if(this->fd_control >= 0) {
      printf_control(this, "RESULT %d %d\r\n", this->token, err);
      err = CONTROL_OK;
    }
    
  } else if(!strncasecmp(cmd, "SEEK ", 5)) {
    if(this->slave_stream) {
      int pos_stream=0, pos_time=0, length_time=0;
      xine_get_pos_length(this->slave_stream, 
			  &pos_stream, &pos_time, &length_time);
      if(cmd[5]=='+')
	pos_time += atoi(cmd+6) * 1000;
      else if(cmd[5]=='-')
	pos_time -= atoi(cmd+6) * 1000;
      else
	pos_time = atoi(cmd+5) * 1000;
      err = xine_play (this->slave_stream, 0, pos_time);
      if(this->fd_control >= 0)
	err = CONTROL_OK;
    }

  } else if(!strncasecmp(cmd, "GETLENGTH", 9)) {
    int pos_stream=0, pos_time=0, length_time=0;
    xine_get_pos_length(stream, &pos_stream, &pos_time, &length_time);
    err = length_time/*/1000*/;
    if(this->fd_control >= 0) {
      printf_control(this, "RESULT %d %d\r\n", this->token, err);
      err = CONTROL_OK;
    }

  } else if(!strncasecmp(cmd, "GETPOS", 6)) {
    int pos_stream=0, pos_time=0, length_time=0;
    xine_get_pos_length(stream, &pos_stream, &pos_time, &length_time);
    err = pos_time/*/1000*/;
    if(this->fd_control >= 0) {
      printf_control(this, "RESULT %d %d\r\n", this->token, err);
      err = CONTROL_OK;
    }

  } else if(!strncasecmp(cmd, "SUBTITLES ", 10)) {
    if(this->slave_stream) {
      int vpos = 0;
      if(1 == sscanf(cmd, "SUBTITLES %d", &vpos))
	stream->xine->config->update_num(stream->xine->config,
		          "subtitles.separate.vertical_offset", vpos);
      else
	err = CONTROL_PARAM_ERROR;
    }

  } else {
    LOGMSG("unknown control %s", cmd);
    err = CONTROL_UNKNOWN;
  }

  LOGCMD("vdr_plugin_parse_control: DONE (%d): %s", err, cmd);

  VDR_ENTRY_UNLOCK();

  return err;
}

static void *vdr_control_thread(void *this_gen)
{
  vdr_input_plugin_t *this = (vdr_input_plugin_t *) this_gen;
  char line[1024];
  int err;
  int counter = 100;

  LOGDBG("Control thread started");

  /*(void)nice(-1);*/

  /* wait until state changes from open to play */
  while(bSymbolsFound && counter>0 && ! this->funcs.fe_control) {
    xine_usec_sleep(50*1000);
    counter--;
  } 

  write_control(this, "CONFIG\r\n");
  
  while(this->control_running && this->fd_control >= 0) {

    /* read next command */
    line[0] = 0;
    pthread_testcancel();
    if((err=readline_control(this, line, sizeof(line)-1)) <= 0) {
      if(err < 0) {
	LOGERR("control stream read error");
	break;
      }
      continue;
    }
    LOGCMD("Received command %s",line);
    pthread_testcancel();
    
    if(!this->control_running || this->fd_control < 0) 
      break;

    /* parse */
    switch(err = vdr_plugin_parse_control(this_gen, line)) {
      case CONTROL_OK:
        break;
      case CONTROL_UNKNOWN:
        LOGMSG("unknown control message %s", line);
        break;
      case CONTROL_PARAM_ERROR:
        LOGMSG("invalid parameter in control message %s", line);
        break;
      case CONTROL_DISCONNECTED:
        LOGERR("control stream read error - disconnected ?");
        this->control_running = 0;
        break;
      default:
        LOGMSG("parse_control failed with result: %d", err);
        break;
    }
  }

  this->control_running = 0;
  LOGDBG("Control thread terminating...");

  pthread_mutex_lock(&this->lock);
  if(this->fd_control >= 0) 
    close(this->fd_control);
  if(this->fd_data >= 0) 
    close(this->fd_data);
  this->fd_data = this->fd_control = -1;
  pthread_mutex_unlock(&this->lock);

  LOGDBG("Control thread terminated");

  pthread_exit(NULL);
}

/**************************** Control to VDR ********************************/

/* Map some xine input events to vdr input (remote key names) */
struct {
  int event;
  char *name;
} vdr_keymap[] = {
  {XINE_EVENT_INPUT_NEXT,     "Next"},
  {XINE_EVENT_INPUT_PREVIOUS, "Previous"},

  {XINE_EVENT_INPUT_DOWN,     "Down"},
  {XINE_EVENT_INPUT_UP,       "Up"},
  {XINE_EVENT_INPUT_LEFT,     "Left"},
  {XINE_EVENT_INPUT_RIGHT,    "Right"},
  {XINE_EVENT_INPUT_SELECT,   "Ok"},

  {XINE_EVENT_INPUT_MENU1,    "Menu"},
  {XINE_EVENT_INPUT_MENU2,    "Red"},
  {XINE_EVENT_INPUT_MENU3,    "Green"},
  {XINE_EVENT_INPUT_MENU4,    "Yellow"},
  {XINE_EVENT_INPUT_MENU5,    "Blue"},
  {XINE_EVENT_INPUT_NUMBER_0, "0"},
  {XINE_EVENT_INPUT_NUMBER_1, "1"},
  {XINE_EVENT_INPUT_NUMBER_2, "2"},
  {XINE_EVENT_INPUT_NUMBER_3, "3"},
  {XINE_EVENT_INPUT_NUMBER_4, "4"},
  {XINE_EVENT_INPUT_NUMBER_5, "5"},
  {XINE_EVENT_INPUT_NUMBER_6, "6"},
  {XINE_EVENT_INPUT_NUMBER_7, "7"},
  {XINE_EVENT_INPUT_NUMBER_8, "8"},
  {XINE_EVENT_INPUT_NUMBER_9, "9"},

  {XINE_EVENT_VDR_BACK,         "Back"},
  {XINE_EVENT_VDR_CHANNELPLUS,  "Channel+"},
  {XINE_EVENT_VDR_CHANNELMINUS, "Channel-"},
  {XINE_EVENT_VDR_RED,          "Red"},
  {XINE_EVENT_VDR_GREEN,        "Green"},
  {XINE_EVENT_VDR_YELLOW,       "Yellow"},
  {XINE_EVENT_VDR_BLUE,         "Blue"},
  {XINE_EVENT_VDR_PLAY,         "Play"},
  {XINE_EVENT_VDR_PAUSE,        "Pause"},
  {XINE_EVENT_VDR_STOP,         "Stop"},
  {XINE_EVENT_VDR_RECORD,       "Record"},
  {XINE_EVENT_VDR_FASTFWD,      "FastFwd"},
  {XINE_EVENT_VDR_FASTREW,      "FastRew"},
  {XINE_EVENT_VDR_POWER,        "Power"},
  {XINE_EVENT_VDR_SCHEDULE,     "Schedule"},
  {XINE_EVENT_VDR_CHANNELS,     "Channels"},
  {XINE_EVENT_VDR_TIMERS,       "Timers"},
  {XINE_EVENT_VDR_RECORDINGS,   "Recordings"},
  {XINE_EVENT_VDR_SETUP,        "Setup"},
  {XINE_EVENT_VDR_COMMANDS,     "Commands"},
  {XINE_EVENT_VDR_USER1,        "User1"},
  {XINE_EVENT_VDR_USER2,        "User2"},
  {XINE_EVENT_VDR_USER3,        "User3"},
  {XINE_EVENT_VDR_USER4,        "User4"},
  {XINE_EVENT_VDR_USER5,        "User5"},
  {XINE_EVENT_VDR_USER6,        "User6"},
  {XINE_EVENT_VDR_USER7,        "User7"},
  {XINE_EVENT_VDR_USER8,        "User8"},
  {XINE_EVENT_VDR_USER9,        "User9"},
  {XINE_EVENT_VDR_VOLPLUS,      "Volume+"},
  {XINE_EVENT_VDR_VOLMINUS,     "Volume-"},
  {XINE_EVENT_VDR_MUTE,         "Mute"},
  {XINE_EVENT_VDR_AUDIO,        "Audio"},
#if XINE_VERSION_CODE > 10101
  {XINE_EVENT_VDR_INFO,         "Info"},
#endif
  {-1, NULL}
};

static void vdr_event_cb (void *user_data, const xine_event_t *event) 
{
  vdr_input_plugin_t *this = (vdr_input_plugin_t *)user_data;
  int i = 0;

  while(vdr_keymap[i].name) {
    if(event->type == vdr_keymap[i].event) {
      if(event->data && event->data_length == 4 && 
	 !strncmp(event->data, "VDR", 4)) {
	/*LOGMSG("Input event created by self, ignoring");*/
	return;
      }
      LOGDBG("XINE_EVENT (input) %d --> %s", 
	     event->type, vdr_keymap[i].name);

      if(this->funcs.input_control) {
	this->funcs.input_control((input_plugin_t *)this, 
				  NULL, vdr_keymap[i].name, 0, 0);
      }
      if(this->funcs.xine_input_event) {
	this->funcs.xine_input_event(NULL, vdr_keymap[i].name);
      }
      return;
    }
    i++;
  }

  switch (event->type) {
    case XINE_EVENT_FRAME_FORMAT_CHANGE:
      {
        xine_format_change_data_t *frame_change = 
	    (xine_format_change_data_t *)event->data;
        LOGOSD("XINE_EVENT_FRAME_FORMAT_CHANGE (%dx%d, aspect=%d)", 
	       frame_change->width, frame_change->height, 
	       frame_change->aspect);
	if(this->rescale_osd) 
	  vdr_scale_osds(this, frame_change->width, frame_change->height);
      }
      break;

    case XINE_EVENT_UI_PLAYBACK_FINISHED:
      if(event->stream == this->stream) {
	LOGMSG("XINE_EVENT_UI_PLAYBACK_FINISHED");
	pthread_mutex_lock(&this->lock);
	this->playback_finished = 1;
	this->control_running = 0;
	if(this->fd_control >= 0)
	  close(this->fd_control);
	if(this->fd_data >= 0)
	  close(this->fd_data);
	this->fd_data = this->fd_control = -1;
	pthread_mutex_unlock(&this->lock);
#if 1
	if(iSysLogLevel > 2) {
	  /* dump whole xine log as we should not be here ... */
	  xine_t *xine = this->stream->xine;
	  int i, j;
	  int logs = xine_get_log_section_count(xine);
	  const char * const * names = xine_get_log_names(xine);
	  for(i=0; i<logs; i++) {
	    const char * const * lines = xine_get_log(xine, i);
	    if(lines[0]) {
	      printf("\nLOG: %s\n",names[i]);
	      for(j=0; lines[j] && *lines[j]; j++)
		printf("  %2d: %s", j, lines[j]);
	    }
	  }
	}
#endif
      } else if(event->stream == this->slave_stream) {
	LOGMSG("XINE_EVENT_UI_PLAYBACK_FINISHED (slave stream)");
	if(this->fd_control >= 0) {
	  write_control(this, "ENDOFSTREAM\r\n");
	} else {
	  if(this->funcs.fe_control) 
	    this->funcs.fe_control(this->funcs.fe_handle, "ENDOFSTREAM\r\n");
#if 0
	  if(!this->loop_play) {
	    /* forward to vdr-fe (listening only VDR stream events) */
	    xine_event_t event;
	    event.data_length = 0;
	    event.type        = XINE_EVENT_UI_PLAYBACK_FINISHED;
	    xine_event_send (this->stream, &event);
	  } else {
# if 0
	    xine_usec_sleep(500*1000);
	    xine_play(this->slave_stream, 0, 0);
# endif
	  }
#endif
	}
      }
      break;

    default:
      LOGCMD("Got an xine event, type 0x%08x", event->type);
      break;
    }
}

/**************************** Data Stream *********************************/

static int vdr_plugin_read_net_tcp(vdr_input_plugin_t *this)
{
  buf_element_t *read_buffer = NULL;
  int cnt = 0, todo = 0, n, result;

  while(XIO_READY == (result = io_select_rd(this->fd_data))) {

    /* Allocate buffer */
    if(!read_buffer) {

      pthread_testcancel();

      read_buffer = get_buf_element(this, 0, 0);
      if(!read_buffer) {
	VDR_ENTRY_LOCK(XIO_ERROR);
	vdr_plugin_poll(this, 100);
	VDR_ENTRY_UNLOCK();

	read_buffer = get_buf_element(this, 0, 0);
	if(!read_buffer) {
	  if(!this->is_paused)
	    LOGDBG("TCP: fifo buffer full");
	  xine_usec_sleep(3*1000);
	  continue; /*  must call select to check fd for errors / closing */
	}
      }

      todo = sizeof(stream_tcp_header_t);
      cnt = 0;
    }

    /* Read data */
    errno = 0;
    n = read(this->fd_data, &read_buffer->mem[cnt], todo-cnt);
    if(n <= 0) {
      if(!n || (errno != EINTR && errno != EAGAIN)) {
	LOGERR("TCP read error (data stream %d : %d)", this->fd_data, n);
	result = XIO_ERROR;
	break;
      }
      continue;
    }

    cnt += n;

    if(cnt == sizeof(stream_tcp_header_t)) {
      /* Header complete */
      stream_tcp_header_t *hdr = ((stream_tcp_header_t *)read_buffer->content);
      hdr->len = ntohl(hdr->len);
      hdr->pos = ntohull(hdr->pos);

      todo = cnt + hdr->len;
      if(todo+cnt >= read_buffer->max_size) {
	LOGMSG("TCP: Buffer too small (%d ; incoming frame %d bytes)", 
	       read_buffer->max_size, todo + cnt);
	todo = read_buffer->max_size - cnt - 1;
      }
    }

    if(cnt >= todo) {
      /* Buffer complete */
      stream_tcp_header_t *hdr = ((stream_tcp_header_t *)read_buffer->content);
      if(hdr->pos == (uint64_t)(-1ULL) /*0xffffffff*/) {
	/* control data */
	uint8_t *pkt_data = read_buffer->content + sizeof(stream_tcp_header_t);
	if(pkt_data[0]) { /* -> can't be pes frame */
	  pkt_data[64] = 0;
	  LOGMSG("Control message in data stream: %s", (char*)pkt_data);
	  vdr_plugin_parse_control((input_plugin_t*)this, (char*)pkt_data);

	  /* read next block */
	  todo = sizeof(stream_tcp_header_t);
	  cnt = 0;
	  continue;
	}
      }
 
      /* frame ready */
      read_buffer->size = cnt;
      read_buffer->type = BUF_MAJOR_MASK;
      this->block_buffer->put(this->block_buffer, read_buffer);
      read_buffer = NULL;
    }
  }

  if(read_buffer) {
    read_buffer->free_buffer(read_buffer);
    if(cnt && this->fd_data >= 0 && result == XIO_TIMEOUT) {
      LOGMSG("TCP: Delay too long, disconnecting");
      close(this->fd_data);
      close(this->fd_control);
      this->fd_data = this->fd_control = -1;
      this->control_running = 0;
    }
  }

  return result;
}

static int vdr_plugin_read_net_udp(vdr_input_plugin_t *this)
{
  struct sockaddr_in server_address;
  socklen_t address_len = sizeof(server_address);
  udp_data_t *udp = this->udp_data;
  stream_udp_header_t *pkt;
  uint8_t *pkt_data;
  int result = XIO_ERROR, n, current_seq, timeouts = 0;
  buf_element_t *read_buffer = NULL;

  while(this->fd_data >= 0) {

    result = _x_io_select(this->stream, this->fd_data,
			  XIO_READ_READY, 20);

    if(result != XIO_READY) {
      if(result == XIO_TIMEOUT) {
	if(timeouts++ > 25)
	  return XIO_TIMEOUT;
	continue;
      }
      return result;
    }
    timeouts = 0;

    /* 
     * allocate buffer and read incoming UDP packet from socket 
     */

    if(!read_buffer) {
      
      pthread_testcancel();

      read_buffer = get_buf_element(this, 0, 0);
      if(!read_buffer) {
	/* if queue is full, skip (video) frame. 
	   Waiting longer for free buffers just makes things worse ... */
	if(!this->is_paused) {
	  LOGDBG("UDP Fifo buffer full !");
	  if(this->scr && !udp->scr_jump_done) {
	    pvrscr_skip_frame (this->scr);
	    LOGMSG("SCR jump: +40 ms (live=%d, tunning=%d) time %ds", 
		   this->live_mode, this->scr_tunning, 
		   (int)(monotonic_time_ms()/1000));
	    udp->scr_jump_done = 50;
	    xine_usec_sleep(5*1000);
	  }
	}

	VDR_ENTRY_LOCK(XIO_ERROR);
	vdr_plugin_poll(this, 100);
	VDR_ENTRY_UNLOCK();

	read_buffer = get_buf_element(this, 0, 0);
	if(!read_buffer) {
	  if(!this->is_paused)
	    LOGMSG("Fifo buffer still full after poll !");
	  xine_usec_sleep(5*1000);
	  return result;
	}
      }

      if(udp->scr_jump_done)
	udp->scr_jump_done --;
    }

    /* Receive frame from socket and check for errors */
    n = recvfrom(this->fd_data, read_buffer->mem, 
		 read_buffer->max_size, MSG_TRUNC,
		 &server_address, &address_len);
    if(n <= 0) {
      LOGERR("read_net_udp recv() error");
      if(!n || errno != EINTR)
	result = XIO_ERROR;
      break;
    }

    /* check source address */
    if((server_address.sin_addr.s_addr != 
	udp->server_address.sin_addr.s_addr) ||
       server_address.sin_port != udp->server_address.sin_port) {
#ifdef LOG_UDP
      uint32_t tmp_ip = ntohl(server_address.sin_addr.s_addr);
      LOGUDP("Received data from unknown sender: %d.%d.%d.%d:%d",
	     ((tmp_ip>>24)&0xff), ((tmp_ip>>16)&0xff), 
	     ((tmp_ip>>8)&0xff), ((tmp_ip)&0xff),
	     server_address.sin_port);
#endif
      continue;
    }

    /* Check if frame size is valid */
    if(n < sizeof(stream_udp_header_t)) {
      LOGMSG("received invalid UDP packet (too short)");
      continue;
    }
    if(n > read_buffer->max_size) {
      LOGMSG("received too large UDP packet ; part of data was discarded");
      n = read_buffer->max_size;
    }
    read_buffer->size = n;
    read_buffer->type = BUF_MAJOR_MASK;

    pkt = (stream_udp_header_t*)read_buffer->mem;
    pkt_data = read_buffer->mem + sizeof(stream_udp_header_t);
    pkt->seq = ntohs(pkt->seq);
    pkt->pos = ntohull(pkt->pos);

    /* Check for control messages */
    if(pkt->seq == (uint16_t)(-1) /*0xffff*/ && 
       pkt->pos == (uint64_t)(-1ULL) /*0xffffffff*/ &&
       pkt_data[0]) { /* -> can't be PES frame */
      pkt_data[64] = 0;
      if(!strncmp((char*)pkt_data, "UDP MISSING", 11)) {
	/* Re-send failed */ 
	int seq1 = 0, seq2 = 0;
	uint64_t rpos = 0ULL;
	sscanf((char*)pkt_data, "UDP MISSING %d-%d %" PRIu64, 
	       &seq1, &seq2, &rpos);
	read_buffer->size = sizeof(stream_udp_header_t);
	read_buffer->type = BUF_MAJOR_MASK;
	pkt->pos = rpos;
	LOGUDP("Got UDP MISSING %d-%d (currseq=%d)", seq1, seq2, udp->next_seq);
	if(seq1 == udp->next_seq) {
	  /* this is the one we are expecting ... */
	  int n = ADDSEQ(seq2 + 1, -seq1);
	  udp->missed_frames += n;
	  seq2 &= UDP_SEQ_MASK;
	  pkt->seq = seq2;
	  udp->next_seq = seq2;
	  LOGUDP("  accepted: now currseq %d", udp->next_seq);
	  /* -> drop frame thru as empty ; it will trigger queue to continue */
	} else {
	  LOGUDP("  rejected: not expected seq ???");
	  continue;
	}
      } else {
	LOGMSG("Control message in data stream: %s", (char*)pkt_data);
	vdr_plugin_parse_control((input_plugin_t*)this, (char*)pkt_data);
	/* #warning some messages should be delayed and executed in read_block
	   (ex. audio/spu stream changes, flush) */
	continue;
      }
    } else {
      /* Check for PES header */
      if(pkt_data[0] || pkt_data[1] || pkt_data[2] != 1) {
	LOGMSG("received invalid UDP packet (PES header 0x000001 missing)");
	continue;
      }
    }

    /* Check if header is valid */
    if(pkt->seq > UDP_SEQ_MASK) {
      LOGMSG("received invalid UDP packet (sequence number too big)");
      continue;
    }

    /*
     * handle re-ordering and retransmissios 
     */

    current_seq = pkt->seq & UDP_SEQ_MASK;
    /* first received frame initializes sequence counter */
    if(udp->received_frames == -1) { 
      udp->next_seq = current_seq;
      udp->received_frames = 0;
    }

    /* check if received sequence number is inside allowed window 
       (half of whole range) */
    if(ADDSEQ(current_seq, -udp->next_seq) > ((UDP_SEQ_MASK+1) >> 1)/*0x80*/) {
      struct sockaddr_in sin;
      LOGUDP("Received SeqNo out of window (%d ; [%d..%d])", 
	     current_seq, udp->next_seq, 
	     (udp->next_seq+((UDP_SEQ_MASK+1) >> 1)/*0x80*/) & UDP_SEQ_MASK);
      /* reset link */
      LOGDBG("UDP: resetting link");
      memcpy(&sin, &udp->server_address, sizeof(sin));
      free_udp_data(udp);
      udp = this->udp_data = init_udp_data();
      memcpy(&udp->server_address, &sin, sizeof(sin));
      continue;
    }

    /* Add received frame to incoming queue */
    if(udp->queue[current_seq]) {
      /* Duplicate packet or lot of dropped packets */
      LOGUDP("Got duplicate or window exceeded ? (queue slot %d in use) !",
	     current_seq);
      udp->queue[current_seq]->free_buffer(udp->queue[current_seq]);
      udp->queue[current_seq] = NULL;
      if(!udp->queued) 
	LOGERR("UDP queue corrupt !!!");
      else
	udp->queued--;
    }
    udp->queue[current_seq] = read_buffer;
    read_buffer = NULL;
    udp->queued ++;

    /* stay inside receiving window:
       If window exceeded, skip missing frames */
    if(udp->queued > ((UDP_SEQ_MASK+1)>>2)) {
#ifdef LOG_UDP
      int start = udp->next_seq;
#endif
      while(!udp->queue[udp->next_seq]) {
	INCSEQ(udp->next_seq);
	udp->missed_frames++;
      }
      udp->resend_requested = 0;
      LOGUDP("Re-ordering window exceeded, skipped missed frames %d-%d", 
	     start, udp->next_seq-1);
    }

    /* flush continous part of queue to demuxer queue */
    while(udp->queued > 0 && udp->queue[udp->next_seq]) {
      pkt = (stream_udp_header_t*)udp->queue[udp->next_seq]->content;
      udp->queue_input_pos = pkt->pos + udp->queue[udp->next_seq]->size - 
	                     sizeof(stream_udp_header_t);
      if(udp->queue[udp->next_seq]->size > sizeof(stream_udp_header_t))
	this->block_buffer->put(this->block_buffer, udp->queue[udp->next_seq]);
      else
	udp->queue[udp->next_seq]->free_buffer(udp->queue[udp->next_seq]);
      
      udp->queue[udp->next_seq] = NULL;
      udp->queued --;
      INCSEQ(udp->next_seq);
      if(udp->resend_requested)
	udp->resend_requested --;
    }

    /* no new resend requests until previous has been completed or failed */
    if(udp->resend_requested) 
      continue;

    /* If frames are missing, request re-send */
    if(NEXTSEQ(current_seq) != udp->next_seq  &&  udp->queued) {

      if(!udp->resend_requested) {
	int max_req = 20;

	while(!udp->queue[current_seq] && --max_req > 0) 
	  INCSEQ(current_seq);
	
	printf_control(this, "UDP RESEND %d-%d %" PRIu64 "\r\n", 
		       udp->next_seq, PREVSEQ(current_seq), 
		       udp->queue_input_pos);
	udp->resend_requested = 
	  (current_seq + (UDP_SEQ_MASK+1) - udp->next_seq) & UDP_SEQ_MASK;
	
	LOGUDP("%d-%d missing, requested re-send for %d frames",
	       udp->next_seq, PREVSEQ(current_seq), udp->resend_requested);
      }
    }

#ifdef LOG_UDP
    /* Link quality statistics */
    udp->received_frames++;
    if(udp->received_frames >= 1000) {
      if(udp->missed_frames)
	LOGUDP("packet loss %d.%d%% (%4d/%4d)",
	       udp->missed_frames*100/udp->received_frames, 
	       (udp->missed_frames*1000/udp->received_frames)%10, 
	       udp->missed_frames, udp->received_frames);
      udp->missed_frames = udp->received_frames = 0;
    }
#endif
  }
  
  if(read_buffer)
    read_buffer->free_buffer(read_buffer);

  return result;
}


static void *vdr_data_thread(void *this_gen)
{
  vdr_input_plugin_t *this = (vdr_input_plugin_t *) this_gen;

  LOGDBG("Data thread started");

  (void)nice(-1);

  if(this->udp||this->rtp) {
    while(this->control_running) {
      if(vdr_plugin_read_net_udp(this)==XIO_ERROR)
        break;
      pthread_testcancel();
    }
  } else {
    while(this->control_running) {
      if(vdr_plugin_read_net_tcp(this)==XIO_ERROR)
        break;
      pthread_testcancel();
    }
  }

  this->control_running = 0;
  this->playback_finished = 1;

  pthread_mutex_lock(&this->lock);
  if(this->fd_control >= 0) 
    close(this->fd_control);
  if(this->fd_data >= 0) 
    close(this->fd_data);
  this->fd_data = this->fd_control = -1;
  pthread_mutex_unlock(&this->lock);

  LOGDBG("Data thread terminated");

  pthread_exit(NULL);
}

static int write_slave_stream(vdr_input_plugin_t *this, const char *data, int len)
{
#if 0
  fifo_input_plugin_t *slave;
  buf_element_t *buf;

  TRACE("write_slave_stream (%d bytes)", len); 

  if(!this->pip_stream) {
    LOGMSG("Detected new video stream");
    LOGMSG("  no xine stream yet, trying to create ...");
    vdr_plugin_parse_control(this_gen, "SUBSTREAM 0xE1 50 50 288 196");
  }
  if(!this->pip_stream) {
    LOGMSG("  pip substream: no stream !");
    return -1;
  }
  LOGMSG("  pip substream open, queuing data");

  slave = (fifo_input_plugin_t*)this->pip_stream->input_plugin;  
  if(!slave) {
    LOGMSG("  pip substream: no input plugin !");
    return len;
  }

  buf_element_t *buf = slave->buffer_pool->buffer_pool_try_alloc(slave->buffer_pool);
  if(!buf) {
    LOGMSG("  pip substream: fifo full !");
    return 0;
  }
  if(len > buf->max_size) {
    LOGMSG("  pip substream: buf too small");
    buf->free_buffer(buf);
    return len;
  }

  buf->content = buf->mem;
  buf->size = len;
  buf->type = BUF_DEMUX_BLOCK;
  xine_fast_memcpy(buf->content, data, len);
  slave->buffer->put(slave->buffer, buf);
#endif
  return len;
}

static int vdr_plugin_write(input_plugin_t *this_gen, const char *data, int len)
{
  vdr_input_plugin_t *this = (vdr_input_plugin_t *) this_gen;
  buf_element_t      *buf = NULL;
  static int overflows = 0;

  if(this->slave_stream)
    return len;

  /* slave */
  if(((uint8_t*)data)[3] > 0xe0 && ((uint8_t*)data)[3] <= 0xef) 
    return write_slave_stream(this, data, len);

  TRACE("vdr_plugin_write (%d bytes)", len); 

  VDR_ENTRY_LOCK(0);

  buf = get_buf_element(this, len, 0);
  if(!buf) {
    /* need counter to filter non-fatal overflows
       (VDR is not polling for every PES packet) */
    if(overflows++ > 1)
      LOGMSG("vdr_plugin_write: buffer overflow ! (%d bytes)", len);
    VDR_ENTRY_UNLOCK();
    xine_usec_sleep(5*1000);
    return 0; /* EAGAIN */
  }
  overflows = 0;

  if(len > buf->max_size) {
    LOGMSG("vdr_plugin_write: PES too long (%d bytes, max size "
	   "%d bytes), data ignored !", len, buf->max_size);
    buf->free_buffer(buf);
/* curr_pos will be invalid when this point is reached ! */
    VDR_ENTRY_UNLOCK();
    return len;
  }

  buf->size = len;
  xine_fast_memcpy(buf->content, data, len);
  this->block_buffer->put(this->block_buffer, buf);

  VDR_ENTRY_UNLOCK();

  TRACE("vdr_plugin_write returns %d", len);

  return len;
}

static int vdr_plugin_keypress(input_plugin_t *this_gen, 
			       const char *map, const char *key,
			       int repeat, int release)
{
  vdr_input_plugin_t *this = (vdr_input_plugin_t *) this_gen;
  if(pthread_mutex_lock(&this->lock)) {
    LOGERR("vdr_plugin_keypress: pthread_mutex_lock failed");
    return -1;
  }

  if(key && this->fd_control >= 0) {
    if(map)
      printf_control(this, "KEY %s %s %s %s\r\n", map, key, 
		      repeat?"Repeat":"", release?"Release":"");
    else
      printf_control(this, "KEY %s\r\n", key);
  }

  pthread_mutex_unlock(&this->lock);
  return 0;
}


/******************************* Plugin **********************************/

static void track_audio_stream_change(vdr_input_plugin_t *this, buf_element_t *buf)
{
  /* track audio stream changes */
  int audio_changed = 0;
  if(buf->content[3] >= 0xc0 && buf->content[3] < 0xe0) {
    /* audio */
    if(this->prev_audio_stream_id != (buf->content[3] << 8)) {
      LOGDBG("Audio changed -> %d (%02X)", buf->content[3] - 0xc0, buf->content[3]);
      this->prev_audio_stream_id = buf->content[3] << 8;
      audio_changed = 1;
    }
  }
  else if(buf->content[3] == 0xbd) {
    /* PS1 */
    int PayloadOffset  = buf->content[8] + 9;
    int SubStreamId    = buf->content[PayloadOffset];
    int SubStreamType  = SubStreamId & 0xF0;
    int SubStreamIndex = SubStreamId & 0x1F;
    switch (SubStreamType) {
      case 0x20: /* SPU */
      case 0x30: /* SPU */
	/*LOGMSG("SPU %d", SubStreamId);*/
        break;
      case 0x80: /* AC3 & DTS */
	if(this->prev_audio_stream_id != ((0xbd<<8) | SubStreamId)) {
	  LOGDBG("Audio changed -> AC3 %d (BD:%02X)", SubStreamIndex, SubStreamId);
	  this->prev_audio_stream_id = ((0xbd<<8) | SubStreamId);
	  audio_changed = 1;
	}
	break;
      case 0xA0: /* LPCM */
	if(this->prev_audio_stream_id != ((0xbd<<8) | SubStreamId)) {
	  LOGDBG("Audio changed -> LPCM %d (BD:%02X)", SubStreamIndex, SubStreamId);
	  this->prev_audio_stream_id = ((0xbd<<8) | SubStreamId);
	  audio_changed = 1;
	}
	break;
      default:
	/*LOGMSG("Unknown PS1 substream %d", SubStreamId);*/
	break;
    }
  } else {
    /* no audio */
    return;
  }

  if(audio_changed) {
#if XINE_VERSION_CODE < 10102
#  warning xine-lib is older than 1.1.2. Multiple audio streams are not supported.
#else
    put_control_buf(this->stream->audio_fifo,
		    this->stream->audio_fifo,
		    BUF_CONTROL_RESET_TRACK_MAP);
#endif
#if 0
    put_control_buf(this->stream->audio_fifo,
		    this->stream->audio_fifo,
		    BUF_CONTROL_RESET_DECODER);
    put_control_buf(this->stream->audio_fifo,
		    this->stream->audio_fifo,
		    BUF_CONTROL_START);
#endif
#if 0
    LOGMSG("VDR-Given stream: %04x", this->audio_stream_id);
    if(this->reset_audio_cnt < 1)
      LOGMSG("audio changed, reset cnt still < 1");
    this->reset_audio_cnt --;
    if(this->reset_audio_cnt > 0)
      LOGMSG("audio resetted, reset cnt still > 0");
#endif
  }
}

//#define CACHE_FIRST_IFRAME
#ifdef CACHE_FIRST_IFRAME
#  include "cache_iframe.c"
#endif

static off_t vdr_plugin_read (input_plugin_t *this_gen,
			      char *buf, off_t len) 
{
  vdr_input_plugin_t *this = (vdr_input_plugin_t *) this_gen;
  off_t n, total=0;

  if(this->slave_stream) {
    LOGERR("vdr_plugin_read with slave stream !!!");
    return 0;
  }

  TRACE("vdr_plugin_read: reading %" PRIu64 " bytes...", (uint64_t)len);

  while (total<len) {

    if(!this->curr_buffer) {
      buf_element_t *buf = this->input_plugin.read_block(this_gen, this->stream->video_fifo, len);
      if(!buf) 
	return -1 /*total*/;
      pthread_mutex_lock(&this->lock);
      this->curr_buffer = buf;
      this->curpos -= (uint64_t)this->curr_buffer->size;
    } else
      pthread_mutex_lock(&this->lock);

    n = MIN(this->curr_buffer->size, len-total);

    xine_fast_memcpy(&buf[total], this->curr_buffer->content, n);

    this->curr_buffer->size -= n;
    this->curr_buffer->content += n;
    this->curpos += (uint64_t)n;
    total += n;

    if(this->curr_buffer->size <= 0) {
      this->curr_buffer->free_buffer(this->curr_buffer);
      this->curr_buffer = NULL;
    }
    pthread_mutex_unlock(&this->lock);

    TRACE("vdr_plugin_read: got %" PRIu64 " bytes (%" PRIu64 "/%" PRIu64 " bytes read)", 
	  (uint64_t)n, (uint64_t)total, (uint64_t)len);
  }

  TRACE("vdr_plugin_read returns %" PRIu64 " bytes", (uint64_t)total);

  return total;
}

static buf_element_t *vdr_plugin_read_block (input_plugin_t *this_gen,
					     fifo_buffer_t *fifo, off_t todo) 
{
  vdr_input_plugin_t *this = (vdr_input_plugin_t *) this_gen;
  buf_element_t      *buf  = NULL;

  TRACE("vdr_plugin_read_block");

  if(!this->funcs.push_input_write /* reading from socket */ &&
     !this->control_running) {
    LOGMSG("read_block: no data source, returning NULL");
    return NULL; /* disconnected ? */
  }

#ifdef CACHE_FIRST_IFRAME
  if(NULL != (buf = get_cached_iframe(this)))
    return buf;
#endif

#ifdef ADJUST_SCR_SPEED
  if(pthread_mutex_lock(&this->lock)) {
    LOGERR("read_block: pthread_mutex_lock failed");
    return NULL;
  }
  if( (!this->live_mode && this->fd_control < 0) ||
      this->slave_stream) {
    if(this->scr_tunning)
      reset_scr_tunning(this, this->speed_before_pause);
  } else {
    vdr_adjust_realtime_speed(this);
  }
  pthread_mutex_unlock(&this->lock); 
#endif

  do {
    buf = fifo_buffer_try_get(this->block_buffer);
    if(!buf) {
      struct timespec abstime;
      create_timeout_time(&abstime, 500);
      pthread_mutex_lock(&this->block_buffer->mutex);
      if(this->block_buffer->fifo_size <= 0)
	pthread_cond_timedwait (&this->block_buffer->not_empty, 
				&this->block_buffer->mutex, &abstime);
      pthread_mutex_unlock(&this->block_buffer->mutex);
      if(this->block_buffer->fifo_size <= 0)
	if(NULL != (buf = make_padding_frame(this)))
	  return buf;
      continue;
    }

    /* control buffers go always to demuxer */
    if ((buf->type & BUF_MAJOR_MASK) ==  BUF_CONTROL_BASE)
      return buf;

    pthread_mutex_lock(&this->lock);

    /* Update stream position and remove network headers */
    strip_network_headers(this, buf);

    /* Update stream position */
    this->curpos += buf->size;

    /* Handle discard */
    if(this->discard_index > this->curpos && this->guard_index < this->curpos) {
      pthread_mutex_unlock(&this->lock);
      buf->free_buffer(buf);
      buf = NULL;
      continue;
    }

    /* Send current PTS ? */
    if(this->stream_start) {
      this->send_pts = 1;
      this->stream_start = 0;
    }

    pthread_mutex_unlock(&this->lock);

  } while(!buf);

#ifdef CACHE_FIRST_IFRAME
  cache_iframe(this, buf);
#endif

  track_audio_stream_change(this, buf);

  /* Send current PTS ? */
  if(this->send_pts) {
    int64_t pts = pts_from_pes(buf->content, buf->size);
    if(pts > 0) {
      vdr_x_demux_control_newpts(this->stream, pts, 0);
      this->send_pts = 0;
    }
  }

  TRACE("vdr_plugin_read_block: return data, pos end = %" PRIu64, this->curpos);
  return buf;
}

static off_t vdr_plugin_seek (input_plugin_t *this_gen, off_t offset,
			      int origin) 
{
  return -1; /*this->curpos;*/
}

static off_t vdr_plugin_get_length (input_plugin_t *this_gen) 
{
  return -1;
}

static uint32_t vdr_plugin_get_capabilities (input_plugin_t *this_gen) 
{
  return /*INPUT_CAP_PREVIEW |*/ INPUT_CAP_BLOCK;
}

static uint32_t vdr_plugin_get_blocksize (input_plugin_t *this_gen) 
{
  vdr_input_plugin_t *this = (vdr_input_plugin_t *) this_gen;
  int ret = 2048;

  if(this->block_buffer) {
    pthread_mutex_lock(&this->block_buffer->buffer_pool_mutex);
    if(this->block_buffer->first &&  this->block_buffer->first->size > 0)
      ret = this->block_buffer->first->size;
    pthread_mutex_unlock(&this->block_buffer->buffer_pool_mutex);
  }

  return ret;
}

static off_t vdr_plugin_get_current_pos (input_plugin_t *this_gen)
{
  vdr_input_plugin_t *this = (vdr_input_plugin_t *) this_gen;
  return this->discard_index > this->curpos ? this->discard_index : this->curpos;
}

static void vdr_plugin_dispose (input_plugin_t *this_gen) 
{
  vdr_input_plugin_t *this = (vdr_input_plugin_t *) this_gen;
  int i, local;

  if(!this_gen)
    return;

  LOGDBG("vdr_plugin_dispose");

  local = this->funcs.push_input_write ? 1 : 0;
  memset(&this->funcs, 0, sizeof(this->funcs));

  /* sockets */
  if(!local) {
    if(this->fd_control >= 0)
      shutdown(this->fd_control, SHUT_RDWR);
    if(this->fd_data >= 0)
      shutdown(this->fd_data, SHUT_RDWR);
    if(this->fd_data >= 0)
      if(close(this->fd_data))
	LOGERR("close(fd_data) failed");
    if(this->fd_control >= 0)
      if(close(this->fd_control))
	LOGERR("close(fd_control) failed");
    this->fd_data = this->fd_control = -1;
  }

  /* destroy mutexes (keep VDR out) */
  this->control_running = 0;
  while(pthread_mutex_destroy(&this->vdr_entry_lock) == EBUSY) {
    LOGMSG("vdr_entry_lock busy ...");
    pthread_mutex_lock(&this->vdr_entry_lock);
    pthread_mutex_unlock(&this->vdr_entry_lock);
  }
  while(pthread_mutex_destroy(&this->osd_lock) == EBUSY) {
    LOGMSG("osd_lock busy ...");
    pthread_mutex_lock(&this->osd_lock);
    pthread_mutex_unlock(&this->osd_lock);
  }
  while(pthread_mutex_destroy(&this->lock) == EBUSY) {
    LOGMSG("lock busy ...");
    pthread_mutex_lock(&this->lock);
    pthread_mutex_unlock(&this->lock);
  }

  /* event queue(s) */
  if (this->slave_event_queue) 
    xine_event_dispose_queue (this->slave_event_queue);
  this->slave_event_queue = NULL;
  if (this->event_queue) 
    xine_event_dispose_queue (this->event_queue);
  this->event_queue = NULL;

  /* threads */
  if(!local && this->threads_initialized) {
    void *p;
    pthread_cancel(this->control_thread);
    pthread_join (this->control_thread, &p);
    pthread_cancel(this->data_thread);
    pthread_join (this->data_thread, &p);   
  }

  /* slave stream */
  if (this->slave_stream) {
    if(this->funcs.fe_control) 
      this->funcs.fe_control(this->funcs.fe_handle, "SLAVE 0x0\r\n");
    xine_stop(this->slave_stream);
    xine_close(this->slave_stream);
    xine_dispose(this->slave_stream);
    this->slave_stream = NULL;
  }

  /* OSD */
  for(i=0; i<MAX_OSD_OBJECT; i++) {
    if(this->osdhandle[i] != -1) {
      osd_command_t cmd;
      memset(&cmd,0,sizeof(cmd));
      cmd.cmd = OSD_Close;
      cmd.wnd = i;
      exec_osd_command(this, &cmd);
    }
  }

  /* restore video properties */
  if(this->video_properties_saved)
    set_video_properties(this, -1,-1,-1,-1); /* restore defaults */

  signal_buffer_pool_not_empty(this);
  signal_buffer_not_empty(this);

  /* SCR */
  if (this->scr) {
    this->stream->xine->clock->unregister_scr(this->stream->xine->clock, 
					      &this->scr->scr);
    this->scr->scr.exit(&this->scr->scr);
  }

  free (this->mrl);

  if(this->udp_data)
    free_udp_data(this->udp_data);

  /* fifos */

  /* need to get all buffer elements back before disposing own buffers ... */
  if(this->read_buffer)
    this->read_buffer->free_buffer(this->read_buffer);
  if(this->stream && this->stream->audio_fifo)
    this->stream->audio_fifo->clear(this->stream->audio_fifo);
  if(this->stream && this->stream->video_fifo)
    this->stream->video_fifo->clear(this->stream->video_fifo);

  if(this->iframe_buffer)
    this->iframe_buffer->clear(this->iframe_buffer);
  if(this->block_buffer)
    this->block_buffer->clear(this->block_buffer);
  if(this->big_buffer) 
    this->big_buffer->clear(this->big_buffer);
  if(this->hd_buffer)
    this->hd_buffer->clear(this->hd_buffer);

  if(this->iframe_buffer)
    this->iframe_buffer->dispose(this->iframe_buffer);
  if(this->big_buffer) 
    this->big_buffer->dispose(this->big_buffer);
  if(this->block_buffer)
    this->block_buffer->dispose(this->block_buffer);
  if(this->hd_buffer)
    this->hd_buffer->dispose(this->hd_buffer);

  memset(this, 0, sizeof(this));

  free (this);
}

static char* vdr_plugin_get_mrl (input_plugin_t *this_gen) 
{
  vdr_input_plugin_t *this = (vdr_input_plugin_t *) this_gen;
  return this->mrl;
}

static int vdr_plugin_get_optional_data (input_plugin_t *this_gen,
					 void *data, int data_type) 
{
  return INPUT_OPTIONAL_UNSUPPORTED;
}

static int vdr_plugin_open(input_plugin_t *this_gen)
{
  vdr_input_plugin_t *this = (vdr_input_plugin_t *) this_gen;
  xine_t *xine = this->stream->xine;

  this->event_queue = xine_event_new_queue (this->stream);
  xine_event_create_listener_thread (this->event_queue, vdr_event_cb, this);

  this->buffer_pool = this->stream->video_fifo;

#ifdef ADJUST_SCR_SPEED
  /* enable resample method */
  xine->config->update_num(xine->config,
			   "audio.synchronization.av_sync_method",1);

  /* register our own scr provider */
 {
    uint64_t time;
    time = xine->clock->get_current_time(xine->clock);
    this->scr = pvrscr_init();
    this->scr->scr.start(&this->scr->scr, time);
    xine->clock->register_scr(this->stream->xine->clock, &this->scr->scr);
 }
#endif
  this->scr_tunning = SCR_TUNNING_OFF;
  this->curpos = 0;
  return 1;
}

static int vdr_plugin_open_local (input_plugin_t *this_gen) 
{
  LOGDBG("vdr_plugin_open_local");
  return vdr_plugin_open(this_gen);
}

static void set_recv_buffer_size(int fd, int max_buf)
{
  /* try to have larger receiving buffer */

  /*while(max_buf) {*/
  if(setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &max_buf, sizeof(int)) < 0) {
    LOGERR("setsockopt(SO_RCVBUF,%d) failed", max_buf);
    /*max_buf >>= 1;*/
  } else {
    unsigned int tmp = 0, len = sizeof(int);;
    if(getsockopt(fd, SOL_SOCKET, SO_RCVBUF, &tmp, &len) < 0) {
      LOGERR("getsockopt(SO_RCVBUF,%d) failed", max_buf);
      /*max_buf >>= 1;*/
    } else if(tmp != 2*max_buf) {
      LOGDBG("setsockopt(SO_RCVBUF): got %d bytes", tmp);
      /*max_buf >>= 1;*/
    }
  }
  /*}*/
  max_buf = 256;
  /* not going to send anything, so shrink send buffer ... */
  setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &max_buf, sizeof(int));
}

static int alloc_udp_data_socket(int firstport, int trycount, int *port)
{
  int fd, one = 1;
  struct sockaddr_in name;

  name.sin_family = AF_INET;
  name.sin_port   = htons(firstport);
  name.sin_addr.s_addr = htonl(INADDR_ANY);

  fd = socket(PF_INET, SOCK_DGRAM, 0/*IPPROTO_UDP*/);

  set_recv_buffer_size(fd, KILOBYTE(512));

  if(setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(int)) < 0)
    LOGERR("UDP data stream: setsockopt(SO_REUSEADDR) failed");

  while(bind(fd, (struct sockaddr *)&name, sizeof(name)) < 0) {
    if(!--trycount) {
      LOGMSG("UDP Data stream: bind error, no free port found");
      close(fd);
      return -1;
    }
    LOGERR("UDP Data stream: bind error, port %d: %s",
	   name.sin_port, strerror(errno));
    name.sin_port = htons(++firstport);
  }

  *port = ntohs(name.sin_port);
  return fd;
}

static int connect_control_stream(vdr_input_plugin_t *this, const char *host, 
				  int port, int *client_id)
{
  char tmpbuf[256];
  int fd_control;
  int saved_fd = this->fd_control;

  /* Connect to server */
  this->fd_control = fd_control = _x_io_tcp_connect(this->stream, host, port);

  if(fd_control < 0 || 
     XIO_READY != _x_io_tcp_connect_finish(this->stream, this->fd_control, 
					   3000)) {
    LOGERR("Can't connect to tcp://%s:%d", host, port);
    close(fd_control);
    this->fd_control = saved_fd;
    return -1;
  }

  /* Check server greeting */
  if(readline_control(this, tmpbuf, 256) <= 0) {
    LOGMSG("Server not replying");
    close(fd_control);
    this->fd_control = saved_fd;
    return -1;
  }
  LOGMSG("Server greeting: %s", tmpbuf);
  if(!strncmp(tmpbuf, "Access denied", 13)) {
    LOGMSG("Maybe host address is missing from server-side svdrp.conf ?");
    close(fd_control);
    this->fd_control = saved_fd;
    return -1;
  }
  if(!strstr(tmpbuf, "VDR-") || !strstr(tmpbuf, "xineliboutput-") || !strstr(tmpbuf, "READY")) {
    LOGMSG("Unregonized greeting !");
    close(fd_control);
    this->fd_control = saved_fd;
    return -1;
  }
  /* Check server xineliboutput version */
  if(!strstr(tmpbuf, "xineliboutput-" XINELIBOUTPUT_VERSION " ")) {
    LOGMSG("-----------------------------------------------------------------");
    LOGMSG("WARNING: Client and server versions of xinelibout are different !");
    LOGMSG("         Client version (xine_input_vdr.so) is " XINELIBOUTPUT_VERSION);
    LOGMSG("-----------------------------------------------------------------");
  }

  /* Store our client-id */
  if(readline_control(this, tmpbuf, 256) > 0 &&
     !strncmp(tmpbuf, "CLIENT-ID ", 10)) {
    LOGMSG("Got Client-ID: %s", tmpbuf+10);
    if(client_id)
      if(1 != sscanf(tmpbuf+10, "%d", client_id))
	*client_id = -1;
  } else {
    LOGMSG("Warning: No Client-ID !");
    if(*client_id)
      *client_id = -1;
  }

  /* set socket to non-blocking mode */
  fcntl (fd_control, F_SETFL, fcntl (fd_control, F_GETFL) | O_NONBLOCK);

  this->fd_control = saved_fd;
  return fd_control;
}


static int connect_rtp_data_stream(vdr_input_plugin_t *this)
{
  char cmd[64];
  unsigned int ip0, ip1, ip2, ip3, port;
  int fd=-1, one = 1, retries = 0, n;
  struct sockaddr_in multicastAddress;
  struct ip_mreq mreq;
  struct sockaddr_in server_address, sin;
  socklen_t len = sizeof(sin);
  stream_udp_header_t tmp_udp;

  /* get server IP address */
  if(getpeername(this->fd_control, (struct sockaddr *)&server_address, &len)) {
    LOGERR("getpeername(fd_control) failed");
    /* close(fd); */
    return -1;
  }

  /* request RTP data transport from server */

  LOGDBG("Requesting RTP transport");
  if(_x_io_tcp_write(this->stream, this->fd_control, "RTP\r\n", 5) < 0) {
    LOGERR("Control stream write error");
    return -1;
  }

  cmd[0] = 0;
  if(readline_control(this, cmd, 256) < 8 ||
     strncmp(cmd, "RTP ", 4)) {
    LOGMSG("Server does not support RTP ? (%s)", cmd);
    return -1;
  }

  LOGDBG("Got: %s", cmd);
  if(5 != sscanf(cmd, "RTP %u.%u.%u.%u:%u", &ip0, &ip1, &ip2, &ip3, &port) ||
     ip0>0xff || ip1>0xff || ip2>0xff || ip3>0xff || port>0xffff) {
    LOGMSG("Server does not support RTP ? (%s)", cmd);
    return -1;
  }
  
  LOGMSG("Connecting (data) to rtp://%u.%u.%u.%u:%u ...", 
	 ip0, ip1, ip2, ip3, port);
  multicastAddress.sin_family = AF_INET;
  multicastAddress.sin_port   = htons(port);
  multicastAddress.sin_addr.s_addr = htonl((ip0<<24)|(ip1<<16)|(ip2<<8)|ip3);
  LOGDBG("got address: %s int=0x%x net=0x%x translated=0x%x port=%d",
	 cmd+4, (ip0<<24)|(ip1<<16)|(ip2<<8)|ip3, 
	 htonl((ip0<<24)|(ip1<<16)|(ip2<<8)|ip3),
	 inet_addr("224.0.1.9"), port);

  if((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
    LOGERR("socket() failed");
    return -1;
  }
  set_recv_buffer_size(fd, KILOBYTE(512));

  if(setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(int)) < 0) {
    LOGERR("setsockopt(SO_REUSEADDR) failed");
    close(fd);
    return -1;
  }

  if(bind(fd, (struct sockaddr *)&multicastAddress, 
	  sizeof(multicastAddress)) < 0) {
    LOGERR("bind() to multicast address failed");
    close(fd);
    return -1;
  }

  /* Join to multicast group */

  memset(&mreq, 0, sizeof(mreq));
  mreq.imr_multiaddr.s_addr = multicastAddress.sin_addr.s_addr;
  mreq.imr_interface.s_addr = htonl(INADDR_ANY);
  /*mreq.imr_ifindex = 0;*/
  if(setsockopt(fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq))) {
    LOGERR("setsockopt(IP_ADD_MEMBERSHIP) failed. No multicast in kernel?");
    close(fd);
    return -1;
  }

retry_select:

  /* wait until server sends first RTP packet */

  if( XIO_READY != io_select_rd(fd)) {
    LOGDBG("Requesting RTP transport: RTP poll timeout");
    if(++retries < 10) {
      LOGDBG("Requesting RTP transport");
      if(_x_io_tcp_write(this->stream, this->fd_control, "RTP\r\n", 5) < 0) {
	LOGERR("Control stream write error");
	close(fd);
	return -1;
      }
      goto retry_select;
    }
    LOGMSG("Data stream connection timed out (RTP)");
    close(fd);
    return -1;
  }

retry_recvfrom:

  /* check sender address */

  n = recvfrom(fd, &tmp_udp, sizeof(tmp_udp), 0, &sin, &len);
  if(sin.sin_addr.s_addr != server_address.sin_addr.s_addr) {
    uint32_t tmp_ip = ntohl(sin.sin_addr.s_addr);
    LOGMSG("Received UDP/RTP multicast from unknown sender: %d.%d.%d.%d:%d",
	   ((tmp_ip>>24)&0xff), ((tmp_ip>>16)&0xff), 
	   ((tmp_ip>>8)&0xff), ((tmp_ip)&0xff), 
	   sin.sin_port);

    if(XIO_READY == _x_io_select(this->stream, fd, XIO_READ_READY, 0))
      goto retry_recvfrom;
    if(++retries < 4)
      goto retry_select;
    close(fd);
    return -1;
  }

  /* Succeed */

  this->udp_data = init_udp_data();

  /* store server address */
  memcpy(&this->udp_data->server_address, &sin, sizeof(sin));
  
  return fd;
}


static int connect_udp_data_stream(vdr_input_plugin_t *this)
{
  char cmd[64];
  struct sockaddr_in server_address, sin;
  socklen_t len = sizeof(sin);
  uint32_t  tmp_ip;
  stream_udp_header_t tmp_udp;
  int n, retries = 0, port = -1, fd = -1;

  /* get server IP address */
  if(getpeername(this->fd_control, (struct sockaddr *)&server_address, &len)) {
    LOGERR("getpeername(fd_control) failed");
    /* close(fd); */
    return -1;
  }
  tmp_ip = ntohl(server_address.sin_addr.s_addr);

  LOGDBG("VDR server address: %d.%d.%d.%d", 
	 ((tmp_ip>>24)&0xff), ((tmp_ip>>16)&0xff), 
	 ((tmp_ip>>8)&0xff), ((tmp_ip)&0xff));

  /* allocate UDP socket */
  if((fd = alloc_udp_data_socket(DEFAULT_VDR_PORT, 20, &port)) < 0)
    return -1;

  LOGDBG("my UDP port is: %d", port);


retry_request:

  /* request UDP data transport from server */

  LOGDBG("Requesting UDP transport");
  sprintf(cmd, "UDP %d\r\n", port);
  if(_x_io_tcp_write(this->stream, this->fd_control, cmd, strlen(cmd)) < 0) {
    LOGERR("Control stream write error");
    close(fd);
    return -1;
  }

retry_select:

  /* wait until server sends first UDP packet */

  if( XIO_READY != io_select_rd(fd)) {
    LOGDBG("Requesting UDP transport: UDP poll timeout");
    if(++retries < 4)
      goto retry_request;
    LOGERR("Data stream connection timed out (UDP)");
    close(fd);
    return -1;
  }

retry_recvfrom:

  /* check sender address */

  n = recvfrom(fd, &tmp_udp, sizeof(tmp_udp), 0, &sin, &len);
  if(sin.sin_addr.s_addr != server_address.sin_addr.s_addr) {
    tmp_ip = ntohl(sin.sin_addr.s_addr);
    LOGMSG("Received UDP packet from unknown sender: %d.%d.%d.%d:%d",
	   ((tmp_ip>>24)&0xff), ((tmp_ip>>16)&0xff), 
	   ((tmp_ip>>8)&0xff), ((tmp_ip)&0xff), 
	   sin.sin_port);

    if(XIO_READY == _x_io_select(this->stream, fd, XIO_READ_READY, 0))
      goto retry_recvfrom;
    if(++retries < 4)
      goto retry_select;
    close(fd);
    return -1;
  }

  /* Succeed */

  this->udp_data = init_udp_data();

  /* store server address */
  memcpy(&this->udp_data->server_address, &sin, sizeof(sin));
  
  return fd;
}

static int vdr_plugin_open_net (input_plugin_t *this_gen) 
{
  vdr_input_plugin_t *this = (vdr_input_plugin_t *) this_gen;
  int err;
  char tmpbuf[256];

  LOGDBG("vdr_plugin_open_net %s", this->mrl);

  if((!strncasecmp(this->mrl, "xvdr:tcp://", 11) && 1==(this->tcp=1)) ||
     (!strncasecmp(this->mrl, "xvdr:udp://", 11) && 1==(this->udp=1)) ||
     (!strncasecmp(this->mrl, "xvdr:rtp://", 11) && 1==(this->rtp=1)) ||
     (!strncasecmp(this->mrl, "xvdr://", 7))) {
    char *phost = strdup(strstr(this->mrl, "//") + 2);
    char host[256];
    char *port = strchr(phost, ':');
    int iport;
    int one = 1;
    if(port) *port++ = 0;
    iport = port ? atoi(port) : DEFAULT_VDR_PORT;
    strncpy(host, phost, 254);
    free(phost);
    /* TODO: use multiple input plugins - tcp/udp/file */

    /* connect control stream */

    LOGMSG("Connecting (control) to tcp://%s:%d ...", host, iport);
    this->fd_control = connect_control_stream(this, host, iport,
					      &this->client_id);
    if (this->fd_control < 0) {
      LOGERR("Can't connect to tcp://%s:%d", host, iport);
      return 0;
    }
    setsockopt(this->fd_control, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(int));

    LOGMSG("Connected (control) to tcp://%s:%d", host, iport);

    /* connect data stream */

    /* try pipe ? */

    if(!this->tcp && !this->udp && !this->rtp) {
      LOGMSG("Trying pipe (data) ...");
      _x_io_tcp_write(this->stream, this->fd_control, "PIPE\r\n", 6);
      *tmpbuf=0;
      if(readline_control(this, tmpbuf, 256) >5 &&
	 !strncmp(tmpbuf, "PIPE ", 5) &&
	 strncmp(tmpbuf, "PIPE NONE", 9)) {
	LOGMSG("Connecting (data) to pipe://%s", tmpbuf+5);
	if((this->fd_data = open(tmpbuf+5, O_RDONLY|O_NONBLOCK)) >= 0) {
	  _x_io_tcp_write(this->stream, this->fd_control, "PIPE OPEN\r\n", 11);
	  if(readline_control(this, tmpbuf, 256) >6 &&
	     !strncmp(tmpbuf, "PIPE OK", 7)) {
	    fcntl (this->fd_data, F_SETFL, 
		   fcntl (this->fd_data, F_GETFL) | O_NONBLOCK);
	    this->tcp = this->udp = this->tcp = 0;
	    LOGMSG("Pipe connected (data)");
	  } else {
	    close(this->fd_data);
	    this->fd_data = -1;
	    LOGMSG("Pipe connection failed.");
	  }
	} else {
	  LOGERR("Pipe opening failed");
	}
      } else {
	LOGMSG("Server does not support pipes.");
      }
    }

    /* try RTP ? */

    if(this->fd_data < 0 && !this->udp && !this->tcp) {
      /*LOGMSG("Trying RTP connection ...");*/
      /* flush control buffer (if PIPE was tried first) */
      while(0 < read(this->fd_control, tmpbuf, 255)) ;
      if((this->fd_data = connect_rtp_data_stream(this)) < 0) {
	LOGMSG("connect_rtp_data_stream failed");
	this->rtp = 0;
      } else {
	this->rtp = 1;
	this->tcp = this->udp = 0;
	LOGMSG("Data stream connected (RTP)");
      }
    }

    /* try UDP ? */

    if(this->fd_data < 0 && !this->tcp) {
      /*LOGMSG("Trying UDP connection ...", host);*/
      LOGMSG("Connecting (data) to udp://%s ...", host);
      /* flush control buffer (if RTP was tried first) */
      while(0 < read(this->fd_control, tmpbuf, 255)) ;
      if((this->fd_data = connect_udp_data_stream(this)) < 0) {
	LOGMSG("connect_udp_data_stream failed");
	this->udp = 0;
      } else {
	this->udp = 1;
	this->tcp = this->rtp = 0;
	LOGMSG("Data stream connected (UDP)");
      }
    }

    /* fall back to TCP ? */

    if(this->fd_data < 0) {
      LOGMSG("Connecting (data) to tcp://%s:%d ...", host, iport);
      this->tcp = 0;
      this->fd_data = connect_control_stream(this, host, iport, NULL);
      if(this->fd_data < 0) {
	LOGMSG("Data stream connection failed (TCP)");
      } else {
	set_recv_buffer_size(this->fd_data, KILOBYTE(64));
	fcntl (this->fd_data, F_SETFL, 
	       fcntl (this->fd_data, F_GETFL) | O_NONBLOCK);
	/* flush control buffer (if UDP/RTP was tried first) */
	while(0 < read(this->fd_control, tmpbuf, 255)) ;
	 
	sprintf(tmpbuf, "DATA %d\r\n", this->client_id);
	if(write(this->fd_data, tmpbuf, strlen(tmpbuf)) != (ssize_t)strlen(tmpbuf)) {
	  LOGERR("Data stream connection failed (TCP, write)");
	} else if( XIO_READY != io_select_rd(this->fd_data)) {
	  LOGERR("Data stream connection failed (TCP, select)");
	} else if(read(this->fd_data, tmpbuf, 6) != 6) {
	  LOGERR("Data stream connection failed (TCP, read)");
	} else if(strncmp(tmpbuf, "DATA\r\n", 6)) {
	  LOGMSG("Data stream connection failed (TCP, token)");
	} else {
	  this->tcp = 1;
	}
      }
      if(this->tcp) {
	/* succeed */
	this->rtp = this->udp = 0;
	LOGMSG("Data stream connected (TCP)");
      } else {
	/* failed */
	close(this->fd_data);
	close(this->fd_control);
	this->fd_control = this->fd_data = -1;
	return 0;
      }
    }
    
  } else {
    LOGMSG("Unknown mrl (%s)", this->mrl);
    return 0;
  }

  if(!vdr_plugin_open(this_gen))
    return 0;

  queue_nosignal(this);

  this->control_running = 1;
  if ((err = pthread_create (&this->control_thread,
			     NULL, vdr_control_thread, (void*)this)) != 0) {
    LOGERR("Can't create new thread");
    return 0;
  }
  if ((err = pthread_create (&this->data_thread,
			     NULL, vdr_data_thread, (void*)this)) != 0) {
    LOGERR("Can't create new thread");
    return 0;
  }

  if(!(this->stream->video_out->get_capabilities(this->stream->video_out) &
       VO_CAP_UNSCALED_OVERLAY))
    LOGMSG("WARNING: Video output driver reports it does not support unscaled overlays !");

  this->threads_initialized = 1;

  return 1;
}

/**************************** Plugin class *******************************/

static input_plugin_t *vdr_class_get_instance (input_class_t *cls_gen,
				    xine_stream_t *stream,
				    const char *data) 
{
  vdr_input_class_t  *cls = (vdr_input_class_t *) cls_gen;
  vdr_input_plugin_t *this;
  char               *mrl = (char *) data;
  int                local_mode, i;

  LOGDBG("vdr_class_get_instance");

  if (strncasecmp (mrl, "xvdr:",5))
    return NULL;

  if(!strncasecmp(mrl, "xvdr:slave:0x", 13)) 
    return fifo_class_get_instance(cls_gen, stream, data);  

  this = (vdr_input_plugin_t *) xine_xmalloc (sizeof(vdr_input_plugin_t));
  memset(this, 0, sizeof(vdr_input_plugin_t));

  this->stream       = stream;
  this->mrl          = NULL;
  this->cls          = cls;
  this->event_queue  = NULL;

  this->fd_data      = -1;
  this->fd_control   = -1;
  this->udp          = 0;
  this->control_running = 0;
  this->read_buffer  = NULL;

  this->block_buffer = NULL;
  this->curr_buffer  = NULL;
  this->discard_index= 0;
  this->guard_index  = 0;
  this->curpos       = 0;
  this->max_buffers  = 10;

  this->scr          = NULL;

  this->I_frames     = 0;

  this->no_video     = 0;
  this->live_mode    = 0;
  this->still_mode   = 0;
  this->playback_finished = 0;
  this->stream_start = 1;
  this->send_pts     = 0;

  this->rescale_osd  = 0;
  this->unscaled_osd = 0;
  for(i=0; i<MAX_OSD_OBJECT; i++)
    this->osdhandle[i] = -1;;
  this->video_width  = this->vdr_osd_width  = 720;
  this->video_height = this->vdr_osd_height = 576;

  this->video_properties_saved = 0;
  this->orig_hue        = 0;
  this->orig_brightness = 0;
  this->orig_saturation = 0;
  this->orig_contrast   = 0;
  local_mode            = ( (!strncasecmp(mrl, "xvdr://", 7)) && 
			    (strlen(mrl)==7))
                          || (!strncasecmp(mrl, "xvdr:///", 8));

  this->input_plugin.open = local_mode ? vdr_plugin_open_local 
                                       : vdr_plugin_open_net;
  this->input_plugin.get_mrl           = vdr_plugin_get_mrl;
  this->input_plugin.dispose           = vdr_plugin_dispose;
  this->input_plugin.input_class       = cls_gen;

  this->input_plugin.get_capabilities  = vdr_plugin_get_capabilities;
  this->input_plugin.read              = vdr_plugin_read;
  this->input_plugin.read_block        = vdr_plugin_read_block;
  this->input_plugin.seek              = vdr_plugin_seek;
  this->input_plugin.get_current_pos   = vdr_plugin_get_current_pos;
  this->input_plugin.get_length        = vdr_plugin_get_length;
  this->input_plugin.get_blocksize     = vdr_plugin_get_blocksize;
  this->input_plugin.get_optional_data = vdr_plugin_get_optional_data;
  
  if(local_mode) {
    this->funcs.push_input_write  = vdr_plugin_write;
    this->funcs.push_input_control= vdr_plugin_parse_control;
    this->funcs.push_input_osd    = vdr_plugin_exec_osd_command;
    /*this->funcs.xine_input_event= NULL; -- frontend sets this */
  } else {
    this->funcs.input_control     = vdr_plugin_keypress;
  }
  
  this->mrl          = strdup(mrl);
  
  /* buffer */
  this->block_buffer = _x_fifo_buffer_new(4,0x10000+64); /* dummy buf to be used before first read and for big PES frames */
  this->big_buffer = NULL;   /* dummy buf to be used for jumbo PES frames */
  this->hd_buffer = NULL;
  
  /* sync */
  pthread_mutex_init (&this->lock, NULL);
  pthread_mutex_init (&this->osd_lock, NULL);
  pthread_mutex_init (&this->vdr_entry_lock, NULL);

  this->udp_data = NULL;

  LOGDBG("vdr_class_get_instance done.");

  return &this->input_plugin;
}

/*
 * vdr input plugin class stuff
 */

static char *vdr_class_get_description (input_class_t *this_gen) 
{
  return _("VDR (Video Disk Recorder) input plugin");
}

static const char *vdr_class_get_identifier (input_class_t *this_gen) 
{
  return "xvdr";
}

static void vdr_class_dispose (input_class_t *this_gen) 
{
  vdr_input_class_t *cls = (vdr_input_class_t *) this_gen;
  free (cls);
}

static void *init_class (xine_t *xine, void *data) 
{
  vdr_input_class_t  *this;

  this = (vdr_input_class_t *) xine_xmalloc (sizeof (vdr_input_class_t));

  this->xine   = xine;

  this->input_class.get_instance       = vdr_class_get_instance;
  this->input_class.get_identifier     = vdr_class_get_identifier;
  this->input_class.get_description    = vdr_class_get_description;
  this->input_class.get_dir            = NULL;
  this->input_class.get_autoplay_list  = NULL;
  this->input_class.dispose            = vdr_class_dispose;
  this->input_class.eject_media        = NULL;

  SetupLogLevel();

  LOGDBG("init class succeeded");

  return this;
}


/*
 * exported plugin catalog entry
 */

const plugin_info_t xine_plugin_info[] __attribute__((visibility("default"))) = {
  /* type, API, "name", version, special_info, init_function */
  { PLUGIN_INPUT, INPUT_PLUGIN_IFACE_VERSION, "XVDR", XINE_VERSION_CODE, NULL, init_class },
  { PLUGIN_NONE, 0, "", 0, NULL, NULL }
};

const plugin_info_t *xine_plugin_info_xvdr = xine_plugin_info;


