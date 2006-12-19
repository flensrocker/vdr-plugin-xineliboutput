/*
 * xine_frontend_lirc.c
 *
 * Forward (local) lirc keys to VDR (server)
 *
 *
 * Almost directly copied from vdr-1.4.3-2 (lirc.c : cLircRemote) 
 *
 * $Id: xine_frontend_lirc.c,v 1.4 2006-12-19 08:48:27 phintuka Exp $
 *
 */

#include <stdint.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/un.h>

#define REPEATDELAY     350 /* ms */
#define REPEATFREQ      100 /* ms */
#define REPEATTIMEOUT   500 /* ms */
#define RECONNECTDELAY 3000 /* ms */

#define LIRC_KEY_BUF      30
#define LIRC_BUFFER_SIZE 128 
#define MIN_LIRCD_CMD_LEN  5

/* static data */
static pthread_t lirc_thread;
static volatile char *lirc_device_name = NULL;
static volatile int fd_lirc = -1;
static int lirc_repeat_emu = 0;

static uint64_t time_ms()
{
  struct timeval t;
  if (gettimeofday(&t, NULL) == 0)
     return ((uint64_t)t.tv_sec) * 1000ULL + t.tv_usec / 1000ULL;
  return 0;
}

static uint64_t elapsed(uint64_t t)
{
  return time_ms() - t;
}

static void lircd_connect(void)
{
  struct sockaddr_un addr;

  if(fd_lirc >= 0) {
    close(fd_lirc);
    fd_lirc = -1;
  }

  if(!lirc_device_name) {
    LOGDBG("no lirc device given");
    return;
  }

  addr.sun_family = AF_UNIX;
  strcpy(addr.sun_path, (char*)lirc_device_name);

  if ((fd_lirc = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
    LOGERR("lirc error: socket() < 0");
    return;
  }

  if (connect(fd_lirc, (struct sockaddr *)&addr, sizeof(addr))) {
    LOGERR("lirc error: connect(%s) < 0", lirc_device_name);
    close(fd_lirc);
    fd_lirc = -1;
    return;
  }
}

void *lirc_receiver_thread(void *fe)
{
  int timeout = -1;
  uint64_t FirstTime = time_ms();
  uint64_t LastTime = time_ms();
  char buf[LIRC_BUFFER_SIZE];
  char LastKeyName[LIRC_KEY_BUF] = "";
  int repeat = 0;

  LOGMSG("lirc forwarding started");

  nice(-1);
  lircd_connect();

  while(lirc_device_name && fd_lirc >= 0) {
    fd_set set;
    int ready, ret = -1;
    FD_ZERO(&set);
    FD_SET(fd_lirc, &set);

    if (timeout >= 0) {
      struct timeval tv;
#if 0
      if(TimeoutMs < 100)
	TimeoutMs = 100; 
#endif
      tv.tv_sec  = timeout / 1000;
      tv.tv_usec = (timeout % 1000) * 1000;
      ready = select(FD_SETSIZE, &set, NULL, NULL, &tv) > 0 && FD_ISSET(fd_lirc, &set);
    } else {
      ready = select(FD_SETSIZE, &set, NULL, NULL, NULL) > 0 && FD_ISSET(fd_lirc, &set);
    }

    if(ready < 0) {
      LOGMSG("LIRC connection lost ?");
      break;
    }
    
    if(ready) {

      do { 
	errno = 0;
	ret = read(fd_lirc, buf, sizeof(buf));
      } while(ret < 0 && errno == EINTR);

      if (ret <= 0 ) {
	/* try reconnecting */
	LOGERR("LIRC connection lost");
	lircd_connect();
	while(lirc_device_name && fd_lirc < 0) {
	  sleep(RECONNECTDELAY/1000);
	  lircd_connect();
	}
	if(fd_lirc >= 0)
	  LOGMSG("LIRC reconnected");
	continue;
      }

      if (ret >= MIN_LIRCD_CMD_LEN) {
        unsigned int count;
	char KeyName[LIRC_KEY_BUF];
	LOGDBG("LIRC: %s", buf);

	if (sscanf(buf, "%*x %x %29s", &count, KeyName) != 2) { 
	  /* '29' in '%29s' is LIRC_KEY_BUF-1! */
	  LOGMSG("unparseable lirc command: %s", buf);
	  continue;
	}

	if(lirc_repeat_emu)
          if (strcmp(KeyName, LastKeyName) == 0 && elapsed(LastTime) < REPEATDELAY)
	    count = repeat + 1;

        if (count == 0) {
          if (strcmp(KeyName, LastKeyName) == 0 && elapsed(FirstTime) < REPEATDELAY) 
	    continue; /* skip keys coming in too fast */
	  if (repeat)
	    if(find_input((fe_t*)fe))
	      process_xine_keypress(((fe_t*)fe)->input, "LIRC", LastKeyName, 0, 1);
	  /* Put(LastKeyName, false, true);  code, repeat, release */
	  strcpy(LastKeyName, KeyName);
	  repeat = 0;
	  FirstTime = time_ms();
	  timeout = -1;
	}
	else {
	  if (elapsed(LastTime) < REPEATFREQ)
	    continue; /* repeat function kicks in after a short delay */

	  if (elapsed(FirstTime) < REPEATDELAY) {
	    if(lirc_repeat_emu)
	      LastTime = time_ms();
	    continue; /* skip keys coming in too fast */
	  }
	  repeat = 1;
	  timeout = REPEATDELAY;
	}
	LastTime = time_ms();


	if(find_input((fe_t*)fe))
	  process_xine_keypress(((fe_t*)fe)->input, "LIRC", KeyName, repeat, 0);

	/*Put(KeyName, repeat);*/


      }
      else if (repeat) { /* the last one was a repeat, so let's generate a release */
	if (elapsed(LastTime) >= REPEATTIMEOUT) {
	  if(find_input((fe_t*)fe))
	    process_xine_keypress(((fe_t*)fe)->input, "LIRC", LastKeyName, 0, 1);
	  /* Put(LastKeyName, false, true); */
	  repeat = 0;
	  *LastKeyName = 0;
	  timeout = -1;
	}
      }

    }
  }


  if(fd_lirc >= 0)
    close(fd_lirc);
  fd_lirc = -1;
  pthread_exit(NULL);
  return NULL; /* never reached */
}
