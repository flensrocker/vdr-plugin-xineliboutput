/*
 * frontend_local.c:
 *
 * See the main source file 'xineliboutput.c' for copyright information and
 * how to reach the author.
 *
 * $Id: frontend_local.c,v 1.2 2006-06-04 08:18:18 phintuka Exp $
 *
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <dlfcn.h>

#include <vdr/config.h>
#include <vdr/tools.h>

#include "logdefs.h"
#include "config.h"

#include "xine_frontend.h"

#include "frontend_local.h"

//------------------------------ cRwLockBlock ---------------------------------

class cRwLockBlock 
{
  private:
    cRwLock& m_Lock;

  public:
    cRwLockBlock(cRwLock& lock, bool write) : m_Lock(lock)  
      { m_Lock.Lock(write);}

    ~cRwLockBlock()  
      { m_Lock.Unlock(); }
};

#define LOCK_FE     cRwLockBlock(m_feLock, false)
#define LOCK_FE_WR  cRwLockBlock(m_feLock, true)

//----------------------------- cXinelibLocal --------------------------------

cXinelibLocal::cXinelibLocal(const char *frontend_name) :
  cXinelibThread("Local decoder/display (cXinelibThread)"), m_feLock(true) 
{
  fe = NULL;
  h_fe_lib = NULL;
  m_bReconfigRequest = true;
}

cXinelibLocal::~cXinelibLocal() 
{
  TRACEF("cXinelibLocal::~cXinelibLocal");
  
  Stop();
  if(fe) {
    fe->fe_free(fe);
    fe = NULL;
  }
  if(h_fe_lib) {
    dlclose(h_fe_lib);
  }
}

void cXinelibLocal::Stop(void)
{
  TRACEF("cXinelibLocal::Stop");

  SetStopSignal();

  if(1) {
    LOCK_FE;
    m_bReady=false;
    if(fe)
      fe->fe_interrupt(fe);
  }

  cXinelibThread::Stop();
}

//
// Data transfer
//

int cXinelibLocal::Play_PES(const uchar *data, int len)
{
  TRACEF("cXinelibLocal::Play_PES");
  LOCK_FE;
  if(fe) {
    //m_bLoopPlay = false;
    int done = fe->xine_queue_pes_packet(fe, (char*)data, len);
    if(done>0) {
      Lock();
      m_StreamPos += done;
      Unlock();
    } else {
      cCondWait::SleepMs(5);
    }
    return done;
  }

  return 0;
}

void cXinelibLocal::OsdCmd(void *cmd)
{
  TRACEF("cXinelibLocal::OsdCmd");
  if(cmd) {
    LOCK_FE;
    if(fe)
      fe->xine_osd_command(fe, (struct osd_command_s*)cmd);
  }
}

uchar *cXinelibLocal::GrabImage(int &Size, bool Jpeg, 
				 int Quality, int SizeX, 
				 int SizeY)
{
  uchar *data;
  LOCK_FE;
  if(fe && fe->grab)
    if((data = (uchar*)fe->grab(fe, &Size, Jpeg, Quality, SizeX, SizeY)))
      return data;
  return NULL;
}

int64_t cXinelibLocal::GetSTC()
{
  TRACEF("cXinelibLocal::GetSTC");

  int64_t pts = -1;
  char buf[32] = {0};
  strcpy(buf, "GETSTC\r\n");

  LOCK_FE;
  if(fe)
    if(0 == fe->xine_control(fe, (char*)buf))
      //if(*((int64_t *)buf) < MAX_SCR)
      //  if(*((int64_t *)buf) >= 0LL)
	  pts = *((int64_t *)buf);
  return pts;
}

//
// Configuration
//

void cXinelibLocal::ConfigureWindow(int fullscreen, int width, int height, 
				    int modeswitch, char *modeline, 
				    int aspect, int scale_video, 
				    int field_order) 
{
  LOCK_FE;
  if(fe)
    fe->fe_display_config(fe, width, height, fullscreen, modeswitch, modeline, 
			  aspect, scale_video, field_order);
}

void cXinelibLocal::ConfigureDecoder(int pes_buffers, int priority)
{
  // needs xine restart
  {
    LOCK_FE;
    xc.pes_buffers = pes_buffers;
    xc.decoder_priority = priority;
    if(!fe)
      return;    
    m_bReady=false;
    m_bReconfigRequest=true;
    fe->fe_interrupt(fe);    
  }

  while(!m_bReady && !GetStopSignal()) {
    cCondWait::SleepMs(100);
    pthread_yield();
  }

  cCondWait::SleepMs(100);
}

//
// Xine control
//

int cXinelibLocal::Xine_Control(const char *cmd)
{
  TRACEF("cXinelibLocal::Xine_Control");
  char buf[256];
  sprintf(buf, "%s\r\n", cmd);
  LOCK_FE;
  if(fe)
    return fe->xine_control(fe, (char*)buf);
  return 0;
}

//
// keyboard control handler (C callback)
//

extern "C" {
  static void keypress_handler(const char *keymap, const char *key)
  {
    if(!xc.use_x_keyboard || !key) {
      /* Only X11 key events came this way in local mode.
	 Keyboard is handled by vdr. */
      LOGMSG("keypress_handler(%s): X11 Keyboard disabled in config", key);
      return;
    }
    cXinelibThread::KeypressHandler(keymap, key, false, false);
  }
};

//
// Frontend loader
//

frontend_t *cXinelibLocal::load_frontend(const char *fe_name)
{
  Dl_info info;
  struct stat statbuffer;
  char libname[4096]="";
  void *lib = NULL;
  fe_creator_f *fe_creator = NULL;
  static int my_marker = 0;

  if(!dladdr((void *)&my_marker, &info)) {
    LOGERR("Error searching plugin: dladdr() returned false (%s)",dlerror());
    return NULL;
  }
  LOGDBG("xineliboutput: plugin file is %s", info.dli_fname);

  int  fe_ind = strstra(fe_name, xc.s_frontends, FRONTEND_NONE);
  bool fe_try = false;
  if(fe_ind == FRONTEND_NONE) {
    LOGMSG("Front-end %s unknown!", fe_name);
    fe_ind = 0;
    fe_try = true;
  }

  strcpy(libname, info.dli_fname);
  if(strrchr(libname, '/')) 
    *(strrchr(libname, '/')+1) = 0;
  
  LOGMSG("Searching frontend %s from %s", xc.s_frontends[fe_ind], libname);

  do {
    strcat(libname, xc.s_frontend_files[fe_ind]);
    LOGMSG("Probing %s", libname);

    if (stat(libname, &statbuffer)) {
      LOGERR("load_frontend: can't stat %s",libname);
    } else if((statbuffer.st_mode & S_IFMT) != S_IFREG) {
      LOGMSG("load_frontend: %s not regular file ! trying to load anyway ...",
	     libname);
    }

    if( !(lib = dlopen (libname, RTLD_LAZY | RTLD_GLOBAL))) {
      char *dl_error_msg = dlerror();
      LOGERR("load_frontend: cannot dlopen file %s: %s", 
	     libname, dl_error_msg);
    } else if ( (fe_creator = (fe_creator_f*)dlsym(lib, "fe_creator"))) {
      LOGDBG("load_frontend: entry at %p", fe_creator);
      frontend_t *fe = (**fe_creator)();

      if(fe) {
	if(h_fe_lib)
	  dlclose(h_fe_lib);
	h_fe_lib = lib;

	LOGMSG("Using frontend %s (%s) from %s", 
	       xc.s_frontends[fe_ind], xc.s_frontendNames[fe_ind], 
	       xc.s_frontend_files[fe_ind]);
	
	return fe;
      } else {
	LOGMSG("Frontend %s (%s) creation failed",
	       xc.s_frontends[fe_ind], xc.s_frontendNames[fe_ind]);
      }
    } else {
      LOGERR("Frontend entry point not found");
      dlclose(lib);
    }
    
    fe_ind++;  // try next frontend ...

  } while(fe_try && fe_ind < FRONTEND_count);

  LOGMSG("No usable frontends found, giving up !");  
  return NULL;
}

//
// Thread main loop
//

void cXinelibLocal::Action(void) 
{
  frontend_t *curr_fe = NULL;

  TRACEF("cXinelibLocal::Action");

  SetPriority(2); /* lower priority */
#if 0
  // set priority
  sched_param temp;
  temp.sched_priority = 2;

  if (!pthread_setschedparam(pthread_self(), SCHED_RR, &temp)) {
    TRACE("cXinelibLocal("<<getpid()<<"): priority set successful to "
	  << temp.sched_priority << 
	  " [" << sched_get_priority_min(SCHED_RR) << ","
	  << sched_get_priority_max(SCHED_RR) << "]");
  } else {
    TRACE("cXinelibLocal(" << getpid() << "): Error: can't set priority to "
	  << temp.sched_priority << " [" 
	  << sched_get_priority_min(SCHED_RR)
	  << "," << sched_get_priority_max(SCHED_RR) << "]");
  }
  errno = 0;
#endif

  // init frontend
  if(!curr_fe) {
    curr_fe = load_frontend(xc.local_frontend);
    if(!curr_fe) {
      LOGMSG("cXinelibLocal: Error initializing frontend");
      SetStopSignal();
    } else {
      LOGDBG("cXinelibLocal::Action - fe created");
      if(!curr_fe->fe_display_open(curr_fe, xc.width, xc.height, xc.fullscreen,
				   xc.modeswitch, xc.modeline, 
				   xc.display_aspect, keypress_handler, 
				   xc.video_port, xc.scale_video, 
				   xc.field_order)) {
	LOGMSG("cXinelibLocal: Error initializing display");
	SetStopSignal();
      } else {
	LOGDBG("cXinelibLocal::Action - fe->fe_display_open ok");
      }
    }
  }

  // main loop
  while (!GetStopSignal()) {

    {
      // init and start xine engine
      LOCK_FE_WR;
      LOGDBG("cXinelibLocal::Action - xine_init");
      
      fe = curr_fe;
      if(m_bReconfigRequest) {
	if(!fe->xine_init(fe, xc.audio_driver, xc.audio_port, 
			  xc.video_driver, 
			  xc.pes_buffers, xc.decoder_priority,
			  xc.post_plugins)) {
	  LOGMSG("cXinelibLocal: Error initializing frontend");
	  break;
	}
        LOGDBG("cXinelibLocal::Action - fe->xine_init ok");
        m_bReconfigRequest = false;
      }

      // open (xine) stream
      LOGDBG("cXinelibLocal::Action - xine_open");
      if(!fe->xine_open(fe, NULL)) {
        LOGMSG("cXinelibLocal: Error opening xvdr://");
        break;
      }
      LOGDBG("cXinelibLocal::Action - fe->xine_open ok");

      // start playing (xine) stream
      if(!fe->xine_play(fe)) {
        LOGMSG("cXinelibLocal: Error playing xvdr://");
        break;
      }
      LOGDBG("cXinelibLocal::Action - fe->xine_play ok");

      m_StreamPos = 0;
      Xine_Control("STREAMPOS 0"); 
    }

    // configure frontend and xine
    m_bNoVideo  = false;
    ConfigureOSD(xc.prescale_osd, xc.unscaled_osd);
    ConfigurePostprocessing(xc.deinterlace_method, xc.audio_delay, 
			    xc.audio_compression, xc.audio_equalizer,
			    xc.audio_surround);
    ConfigureVideo(xc.hue, xc.saturation, xc.brightness, xc.contrast);
    ConfigurePostprocessing("upmix",     xc.audio_upmix ? true : false, NULL);
#ifdef ENABLE_TEST_POSTPLUGINS
    ConfigurePostprocessing("autocrop",  xc.autocrop  ? true : false, NULL);
    ConfigurePostprocessing("headphone", xc.headphone ? true : false, NULL);
#endif
    LOGDBG("cXinelibLocal::Action - fe config OK");

    LogoDisplay();
    LOGDBG("cXinelibLocal::Action - logo sent");

    {
      LOCK_THREAD;
      Xine_Control("NOVIDEO 0");
      Xine_Control("LIVE 0");
      m_bNoVideo  = false;
      m_bLiveMode = false;
      m_bReady    = true;
    }

    // main event loop
    LOGDBG("cXinelibLocal:Action - Starting event loop");   
    {
      LOCK_FE;
      while(!GetStopSignal() && m_bReady && 
	    (/*m_bLoopPlay ||*/ !fe->xine_is_finished(fe)) && 
	    fe->fe_run(fe)) 
	/*cCondWait::SleepMs(50)*/ ;
    }

    LOGDBG("cXinelibLocal::Action - event loop terminated, "
	   "xine_is_finished=%d", fe->xine_is_finished(fe));

    {
      LOCK_THREAD;
      m_bReady = false;
      m_bEndOfStreamReached = true;
    }

    {
      LOCK_FE_WR;
      if(fe)
        fe->xine_close(fe);
      fe = NULL;
    }

    LOGMSG("cXinelibLocal::Action - Xine closed");
  }

  if(curr_fe) {
    curr_fe->xine_exit(fe);
    curr_fe->fe_display_close(curr_fe);
    curr_fe->fe_free(curr_fe);
  }

  m_bIsFinished = true;
  LOGMSG("cXinelibLocal::Action - finished");
}

