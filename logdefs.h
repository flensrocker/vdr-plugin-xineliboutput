/*
 * logdefs.h: Logging and debug output
 *
 * See the main source file 'xineliboutput.c' for copyright information and
 * how to reach the author.
 *
 * $Id: logdefs.h,v 1.4 2006-10-23 19:22:14 phintuka Exp $
 *
 */

#ifndef _LOGDEFS_H_

#ifndef LOG_MODULENAME
#  define LOG_MODULENAME "[xine..put] "
#endif

#define TRACELINE LOGDBG("at %s:%d %s", __FILE__, __LINE__, __FUNCTION__)


#if defined(XINELIBOUTPUT_DEBUG_STDOUT)
#  /* logging --> stdout */
#  include <stdio.h>
#  define x_syslog(l,x...) do { printf(LOG_MODULENAME x); \
                                printf("\n"); \
                           } while(0)

#elif defined(XINELIBOUTPUT_DEBUG_STDERR)
#  /* logging --> stderr */
#  include <stdio.h>
#  define x_syslog(l,x...) do { fprintf(stderr, LOG_MODULENAME x); \
                                fprintf(stderr,"\n"); \
                           } while(0)

#else
#  /* logging --> syslog */
#  include <syslog.h>
#  if defined(VDRVERSNUM) && VDRVERSNUM >= 10343
#    define x_syslog(l,x...) syslog_with_tid(l, LOG_MODULENAME x)
#  else
#    define x_syslog(l,x...) syslog(l, LOG_MODULENAME x)
#  endif
#endif

#include <errno.h>

#define LOG_ERRNO  x_syslog(LOG_ERR, "ERROR (%s,%d): %s", \
                   __FILE__, __LINE__, strerror(errno))

#define LOGERR(x...) do { \
                       if(SysLogLevel > 0) { \
                          x_syslog(LOG_ERR, x); \
                          if(errno) \
                            LOG_ERRNO; \
                       } \
                     } while(0)
#define LOGMSG(x...) do{ if(SysLogLevel > 1) x_syslog(LOG_INFO,  x); } while(0)
#define LOGDBG(x...) do{ if(SysLogLevel > 2) x_syslog(LOG_DEBUG, x); } while(0)


#ifdef NDEBUG
#  define ASSERT(expr)
#else
#  define ASSERT(expr,fatal) \
      do { \
        if(!(expr)) { \
          LOGERR("Asseretion failed: %s at %s:%d (%s)", \
                 #expr, __FILE__, __LINE__, __FUNCTION__); \
          if(fatal) \
            abort(); \
        } \
      } while(0)
#endif


#ifdef XINELIBOUTPUT_DEBUG
# ifdef __cplusplus
#
#  include <fstream>
#  include <iostream>
#  include <stdio.h>
#
#  ifndef TRACE_IDENT
#    define TRACE_IDENT ""
#  endif
#  if defined(XINELIBOUTPUT_DEBUG_STDOUT)
#    define TRACE(x) do {std::cout << TRACE_IDENT << x << "\n"; fflush(stdout);}while(0)
#  elif defined(XINELIBOUTPUT_DEBUG_STDERR)
#    define TRACE(x) do {std::cerr << TRACE_IDENT << x << "\n"; fflush(stderr);}while(0)
#  else
#    error No trace target !
#  endif
#  define TRACEF(x) cTraceFunctionCall _x_cTraceFunctionCall(x);
   class cTraceFunctionCall {
     public:
       const char *m_name;
       cTraceFunctionCall(const char *name) : m_name(name)
         { TRACE(m_name << " - Enter"); }
       ~cTraceFunctionCall()
         { TRACE(m_name << " - Leave "); }
   };
# endif
#else
#  define TRACE(x)
#  define TRACEF(x)
#endif

#ifdef __cplusplus
#  ifdef TRACK_EXEC_TIME
     class cTimeTracker 
     {
       private:
         const char    *m_Message;
	 const char    *m_Where;
	 uint64_t m_Start;
	 uint64_t m_Trigger;
       public:
	 cTimeTracker(const char *Message, int TriggerMs, const char *Where) {
	   m_Message = Message;
	   m_Where   = Where;
	   m_Trigger = TriggerMs;
	   m_Start   = cTimeMs::Now();      
	 }
	 ~cTimeTracker() {
	   if(cTimeMs::Now() - m_Start > m_Trigger)
	     LOGMSG("********** TimeTracker hit in %s: %d ms %s", 
		    m_Where, (int)(cTimeMs::Now() - m_Start),
		    m_Message?m_Message:"");
	 }
     };
#    define TRACK_TIME(limit) cTimeTracker _timetracker(NULL,limit,__PRETTY_FUNCTION__)
#    define TRACK_TIME_EXT(limit,msg) cTrimeTracker __timetracker(msg,limit,__PRETTY_FUNCTION__)
#  else
#    define TRACK_TIME(limit)
#    define TRACK_TIME_EXT(limit,msg)
#  endif
# endif


#endif /* _LOGDEFS_H_ */
