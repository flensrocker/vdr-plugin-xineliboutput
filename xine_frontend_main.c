/*
 * Simple main() routine for stand-alone frontends.
 *
 * See the main source file 'xineliboutput.c' for copyright information and
 * how to reach the author.
 *
 * $Id: xine_frontend_main.c,v 1.25 2007-01-26 16:00:11 phintuka Exp $
 *
 */

#include <termios.h>
#include <unistd.h>
#include <syslog.h>
#include <getopt.h>
#include <signal.h>

#include "tools/vdrdiscovery.h"

/* include LIRC forwarding code */
#include "xine_frontend_lirc.c"


#if 0
static void xine_log_cb(void *data, int section)
{
  fprintf(stderr, "xine: log section %d\n",section);
}

static void print_xine_log(xine_t *xine)
{
  int i, j;
  int logs = xine_get_log_section_count(xine);
  const char * const * names = xine_get_log_names(xine);
  for(i=0; i<logs; i++) {
    const char * const * lines = xine_get_log(xine, i);
    if(lines[0]) {
      printf("\nLOG: %s\n",names[i]);
      j=-1;
      while(lines[++j] && *lines[++j] )
	printf("  %2d: %s", j, lines[j]);
    }
  }
}
#endif

/* static data */
pthread_t kbd_thread;
struct termios tm, saved_tm;
volatile int terminate_key_pressed = 0;

static int read_key(void)
{
  unsigned char ch;
  int err;
  struct pollfd pfd;
  pfd.fd = STDIN_FILENO;
  pfd.events = POLLIN;

  errno = 0;
  if(1 == (err=poll(&pfd, 1, 50))) {

    if (1 == (err = read(STDIN_FILENO, &ch, 1)))
      return (int)ch;

    if (err < 0)
      LOGERR("read_key: read(stdin) failed");
    else
      LOGERR("read_key: read(stdin) failed: no stdin");
    return -2;

  } else if(err < 0) {
    LOGERR("read_key: poll(stdin) failed");
    return -2;
  }

  return -1;
}

static uint64_t read_key_seq(void)
{
  /* from vdr, remote.c */
  uint64_t k = 0;
  int key1;
  
  if ((key1 = read_key()) >= 0) {
    k = key1;
    if (key1 == 0x1B) {
      // Start of escape sequence
      if ((key1 = read_key()) >= 0) {
	k <<= 8;
	k |= key1 & 0xFF;
	switch (key1) {
	  case 0x4F: // 3-byte sequence
	    if ((key1 = read_key()) >= 0) {
	      k <<= 8;
	      k |= key1 & 0xFF;
	    }
	    break;
	  case 0x5B: // 3- or more-byte sequence
	    if ((key1 = read_key()) >= 0) {
	      k <<= 8;
	      k |= key1 & 0xFF;
	      switch (key1) {
	        case 0x31 ... 0x3F: // more-byte sequence
	        case 0x5B: // strange, may apparently occur
		  do {
		    if ((key1 = read_key()) < 0)
		      break; // Sequence ends here
		    k <<= 8;
		    k |= key1 & 0xFF;
		  } while (key1 != 0x7E);
		  break;
	      }
	    }
	    break;
	}
      }
    }
  }
  if(key1==-2)
    return 0xffff;
  return k;
}

#ifndef IS_FBFE
static void sxfe_toggle_fullscreen(sxfe_t *this);
#endif

static void *kbd_receiver_thread(void *fe) 
{
  uint64_t code = 0;
  char str[64];

  terminate_key_pressed = 0;

  /* Set stdin to deliver keypresses without buffering whole lines */
  tcgetattr(STDIN_FILENO, &saved_tm);
  if (tcgetattr(STDIN_FILENO, &tm) == 0) {
    tm.c_iflag = 0;
    tm.c_lflag &= ~(ICANON | ECHO);
    tm.c_cc[VMIN] = 0;
    tm.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &tm);
  }

  do {
    errno = 0;    
    code = read_key_seq();
    if(code == 0)
      continue;
    if(code == 27) {
      terminate_key_pressed = 1;
      break;
    }
#ifdef XINELIBOUTPUT_FE_TOGGLE_FULLSCREEN
# ifndef IS_FBFE
    if(code == 'f' || code == 'F') {
      sxfe_toggle_fullscreen((sxfe_t*)fe);
      continue;
    } else
# endif
    if(code == 'd' || code == 'D') {
      fe_t *this = (fe_t*)fe;
      xine_set_param(this->stream, XINE_PARAM_VO_DEINTERLACE, 
		     xine_get_param(this->stream, XINE_PARAM_VO_DEINTERLACE) ? 0 : 1);
      continue;
    } else
#endif
    if(code == 0xffff) 
      break;

    snprintf(str, sizeof(str), "%016" PRIX64, code);
    if(find_input((fe_t*)fe))
      process_xine_keypress(((fe_t*)fe)->input, "KBD", str, 0, 0);

  } while(!terminate_key_pressed && code != 0xffff);
  
  LOGDBG("Keyboard thread terminated");
  tcsetattr(STDIN_FILENO, TCSANOW, &saved_tm);
  pthread_exit(NULL);
  return NULL; /* never reached */
}

static void kbd_stop(void)
{
  void *p;

  pthread_cancel (kbd_thread);
  pthread_join (kbd_thread, &p);

  tcsetattr(STDIN_FILENO, TCSANOW, &saved_tm);
}

static void SignalHandler(int signum)
{
  if (signum != SIGPIPE)
    terminate_key_pressed = 1;

  signal(signum, SignalHandler);
}

static char *strcatrealloc(char *dest, const char *src)
{
  int l;

  if (!src || !*src) 
    return dest;

  l = (dest ? strlen(dest) : 0) + strlen(src) + 1;
  if(dest) {
    dest = (char *)realloc(dest, l);
    strcat(dest, src);
  } else {
    dest = (char*)malloc(l);
    strcpy(dest, src);
  } 
  return dest;
}

static const char *help_str = 
"When server address is not given, server is searched from local network.\n"
"If server is not found, localhost (127.0.0.1) is used as default.\n\n"
    "   --help                        Show (this) help message\n"
    "   --audio=audiodriver[:device]  Select audio driver and optional port\n"
    "                                 drivers: auto, alsa, oss, arts, esound, none\n"
    "   --video=videodriver[:device]  Select video driver and optional port\n"
    "                                 X11: auto, x11, xshm, xv, xvmc, xxmc, vidix,\n"
    "                                      none\n"
    "                                 framebuffer: auto, fb, DirectFB, vidixfb,\n"
    "                                      dxr3, aadxr3, none\n"
    "   --display=displayaddress      X11 display address\n"
    "   --aspect=[auto|4:3|16:9|16:10|default] Display aspect ratio\n"
    "   --fullscreen                  Fullscreen mode\n"
    "   --width=x                     Video window width\n"
    "   --height=x                    Video window height\n"
    "   --noscaling                   Disable all video scaling\n"
    "   --post=name[:arg=val[,arg=val]] Load and use xine post plugin(s)\n"
    "                                 examples:\n"
    "                                 --post=upmix\n"
    "                                 --post=upmix;tvtime:enabled=1,cheap_mode=1\n"
    "   --lirc[=devicename]           Use lirc input device\n"
    "                                 Optional lirc socket name can be given\n"
    "   --verbose                     Verbose debug output\n"
    "   --silent                      Silent mode (report only errors)\n"
    "   --syslog                      Write all output to system log\n"
    "   --nokbd                       Disable kayboard input\n"
    "   --daemon                      Run as daemon (disable keyboard,\n"
    "                                 log to syslog and fork to background)\n"
    "   --reconnect                   Automatically reconnect when connection has been lost"
    "   --tcp                         Use TCP transport\n"
    "   --udp                         Use UDP transport\n"
    "   --rtp                         Use RTP transport\n\n"
    "                                 If no transport options are given, transports\n"
    "                                 are tried in following order:\n"
    "                                 local pipe, rtp, udp, tcp\n\n";

static const struct option long_options[] = {
  { "help",       no_argument,       NULL, 'H' },
  { "audio",      required_argument, NULL, 'A' },
  { "video",      required_argument, NULL, 'V' },
  { "display",    required_argument, NULL, 'd' },
  { "aspect",     required_argument, NULL, 'a' },
  { "fullscreen", no_argument,       NULL, 'f' },
  { "width",      required_argument, NULL, 'w' },
  { "height",     required_argument, NULL, 'h' },
  { "noscaling",  no_argument,       NULL, 'n' },
  { "post",       required_argument, NULL, 'P' },
  { "lirc",       optional_argument, NULL, 'L' },

  { "verbose", no_argument,  NULL, 'v' },
  { "silent",  no_argument,  NULL, 's' },
  { "syslog",  no_argument,  NULL, 'l' },
  { "nokbd",   no_argument,  NULL, 'k' },
  { "daemon",  no_argument,  NULL, 'b' },
  
  { "reconnect", no_argument,  NULL, 'R' },
  { "tcp",       no_argument,  NULL, 't' },
  { "udp",       no_argument,  NULL, 'u' },
  { "rtp",       no_argument,  NULL, 'r' },
  { NULL }
};
 

int main(int argc, char *argv[])
{
  char *mrl = NULL, *gdrv = NULL, *adrv = NULL, *adev = NULL;
  int ftcp = 0, fudp = 0, frtp = 0, reconnect = 0, firsttry = 1;
  int fullscreen = 0, width = 720, height = 576;
  int scale_video = 1, aspect = 1;
  int daemon_mode = 0, nokbd = 0;
  char *video_port = NULL;
  int xmajor, xminor, xsub;
  int err, c;
  frontend_t *fe = NULL;
  extern const fe_creator_f fe_creator;
  char *static_post_plugins = NULL;
  char *lirc_dev = NULL;
  int repeat_emu = 0;
  char *exec_name = argv[0];

  if(strrchr(argv[0],'/'))
    exec_name = strrchr(argv[0],'/')+1;

  xine_get_version(&xmajor, &xminor, &xsub);  
  printf("%s %s  (build with xine-lib %d.%d.%d, using xine-lib %d.%d.%d)\n\n",
	 exec_name,
	 FE_VERSION_STR,
	 XINE_MAJOR_VERSION, XINE_MINOR_VERSION, XINE_SUB_VERSION,
	 xmajor, xminor, xsub);

  /* Parse arguments */
  while ((c = getopt_long(argc, argv, "HL:A:V:d:a:fw:h:P:vslkbtur", long_options, NULL)) != -1) {
    switch (c) {
    default:  
    case 'H': printf("\nUsage: %s [options] [xvdr:[udp:|tcp:|rtp:][//host[:port]]] \n"
		     "\nAvailable options:\n", exec_name);
              printf("%s", help_str);
	      exit(0);
    case 'A': adrv = strdup(optarg);
              adev = strchr(adrv, ':');
	      if(adev)
	        *(adev++) = 0;
	      printf("Audio driver: %s\n",adrv);
	      if(adev)
		printf("Audio device: %s\n",adev);
	      break;
    case 'V': gdrv = strdup(optarg);
              if(strchr(gdrv, ':')) {
		video_port = strchr(gdrv, ':');
		*video_port = 0;
		video_port++;
		printf("Video port: %s\n",video_port);
	      }
              printf("Video driver: %s\n",gdrv);
              break;
    case 'd': video_port = strdup(optarg);
              break;
    case 'a': if(!strncmp(optarg, "auto", 4))
                aspect = 0;
              if(!strncmp(optarg, "4:3", 3))
		aspect = 2;
	      if(!strncmp(optarg, "16:9", 4))
		aspect = 3;
	      if(!strncmp(optarg, "16:10", 5))
		aspect = 4;
	      printf("Aspect ratio: %s\n", 
		     aspect==0?"Auto":aspect==2?"4:3":aspect==3?"16:9":
		     aspect==4?"16:10":"Default");
	      break;
    case 'f': fullscreen=1;
              printf("Fullscreen mode\n");
	      break;
    case 'w': width = atoi(optarg);
              printf("Width: %d\n", width);
	      break;
    case 'h': height = atoi(optarg);
              printf("Height: %d\n", height);
	      break;
    case 'n': scale_video = 0;
              printf("Video scaling disabled\n");
	      break;
    case 'P': if(static_post_plugins)
                strcatrealloc(static_post_plugins, ";");
              static_post_plugins = strcatrealloc(static_post_plugins, optarg);
	      printf("Post plugins: %s\n", static_post_plugins);
	      break;
    case 'L': lirc_dev = optarg ? : strdup("/dev/lircd");
              if(strstr((char*)lirc_dev, ",repeatemu")) {
		*strstr((char*)lirc_dev, ",repeatemu") = 0;
		repeat_emu = 1;
              }
              printf("LIRC device:  %s%s\n", lirc_dev,
		     repeat_emu?", emulating key repeat":"");
	      break;
    case 'v': verbose_xine_log = 1;
              SysLogLevel = 3;
	      printf("Verbose mode\n");
	      break;
    case 's': verbose_xine_log = 0;
              SysLogLevel = 1;
	      printf("Silent mode\n");
	      break;
    case 'l': LogToSysLog = 1;
              openlog(exec_name, LOG_PID|LOG_CONS, LOG_USER);
	      break;
    case 'k': nokbd = 1;
              printf("Keyboard input disabled\n");
	      break;
    case 'b': nokbd = daemon_mode = 1;
              printf("Keyboard input disabled\n");
	      break;
    case 'R': reconnect = 1;
              printf("Automatic reconnection enabled\n");
	      break;
    case 't': ftcp = 1;
              printf("Protocol: TCP\n");
	      break;
    case 'u': fudp = 1;
              printf("Protocol: UDP\n");
	      break;
    case 'r': frtp = 1;
              printf("Protocol: RTP\n");
	      break;
    case 1: printf("arg 1 (%s)\n", long_options[optind].name);exit(0);
    }
  }

  if (optind < argc) {
    mrl = strdup(argv[optind]);
    printf("VDR Server: %s\n", mrl);
    while (++optind < argc)
      printf ("Unknown argument: %s\n", argv[optind]);
  }

  printf ("\n");

  /* check xine-lib version */
  if(!xine_check_version(1, 1, 0)) {
    fprintf(stderr,"ERROR: xine-lib is too old, require at least "
	    "xine library version 1.1.0\n");
    return -1;
  }

  /* If server address not given, try to find server automatically */
  if(!mrl || 
     !strcmp(mrl, "xvdr:") ||
     !strcmp(mrl, "xvdr:tcp:") ||
     !strcmp(mrl, "xvdr:udp:") ||
     !strcmp(mrl, "xvdr:rtp:") ||
     !strcmp(mrl, "xvdr:pipe:")) {
    char address[1024] = "";
    int port = -1;
    printf("VDR server not given, searching ...\n");
    if(udp_discovery_find_server(&port, &address[0])) {
      printf("Found VDR server: host %s, port %d\n", address, port);
      if(mrl) {
	char *tmp = mrl;
	mrl = NULL;
	asprintf(&mrl, "%s//%s:%d", tmp, address, port);
	free(tmp);
      } else
	asprintf(&mrl, "xvdr://%s:%d", address, port);
    } else {
      printf("---------------------------------------------------------------\n"
	     "WARNING: MRL not given and server not found from local network.\n"
	     "         Trying to connect to default port on local host.\n"
	     "---------------------------------------------------------------\n");
      mrl = strdup("xvdr://127.0.0.1");
    }
  }

  {
    char *tmp = NULL, *mrl2 = mrl;
    if(frtp && !strstr(mrl, "rtp:"))
      tmp = strdup("xvdr:rtp:");
    else if(fudp && !strstr(mrl, "udp:"))
      tmp = strdup("xvdr:udp:");
    else if(ftcp && !strstr(mrl, "tcp:"))
      tmp = strdup("xvdr:tcp:");
    if(tmp) {
      mrl = strcatrealloc(tmp, strchr(mrl, '/'));
      free(mrl2);
    }
  }

  if(daemon_mode) {
    printf("Entering daemon mode\n\n");
    if (daemon(1, 0) == -1) {
      fprintf(stderr, "%s: %m\n", exec_name);
      LOGERR("daemon() failed");
      return -2;
    }
  }

  /* Create front-end */
  fe = (*fe_creator)();
  if(!fe) {
    printf("Error initializing frontend\n");
    return -3;
  }

  /* Initialize display */
  if(!fe->fe_display_open(fe, width, height, fullscreen, 0, 
			  "", aspect, NULL, video_port, scale_video, 0)) {
    printf("Error opening display\n");
    fe->fe_free(fe);
    return -4;
  }

  /* Initialize xine */
  if(!fe->xine_init(fe, adrv, adev, gdrv, 250, 1, static_post_plugins)) {
    printf("Error initializing xine\n");
    fe->fe_free(fe);
    return -5;
  }

  /* signal handlers */

  if (signal(SIGHUP,  SignalHandler) == SIG_IGN) signal(SIGHUP,  SIG_IGN);
  if (signal(SIGINT,  SignalHandler) == SIG_IGN) signal(SIGINT,  SIG_IGN);
  if (signal(SIGTERM, SignalHandler) == SIG_IGN) signal(SIGTERM, SIG_IGN);
  if (signal(SIGPIPE, SignalHandler) == SIG_IGN) signal(SIGPIPE, SIG_IGN);

  /* Start LIRC forwarding */
  lirc_start((fe_t*)fe, lirc_dev, repeat_emu);
  
  printf("\n\nPress Esc to exit\n\n");
  
  /* Start keyboard listener thread */
  if(!nokbd) {
    if ((err = pthread_create (&kbd_thread,
			       NULL, kbd_receiver_thread, 
			       (void*)fe)) != 0) {
      fprintf(stderr, "can't create new thread for keyboard (%s)\n", 
	      strerror(err));
    }
  }
  
  do {

    if(!firsttry) {
      printf("Connection to server lost. Reconnecting after two seconds...\n");
      sleep(2);
      printf("Reconnecting...\n");
    }

    /* Connect to VDR xineliboutput server */
    if(!fe_xine_open(fe, mrl)) {
      /*print_xine_log(((fe_t *)fe)->xine);*/
      printf("Error opening %s\n", mrl);
      if(!firsttry)
	continue;
      lirc_stop();
      if(!nokbd) kbd_stop();
      fe->fe_free(fe);
      return -6;
    }
  
    if(!fe->xine_play(fe)) {
      /*print_xine_log(((fe_t *)fe)->xine);*/
      /*printf("Error playing %s//%s:%s\n", argv[1], host, port);*/
      printf("Error playing %s\n", argv[1]);
      if(!firsttry)
	continue;
      lirc_stop();
      if(!nokbd) kbd_stop();
      fe->fe_free(fe);
      return -7;
    }
  
    /* Main loop */

    sleep(2);  /* give input_vdr some time to establish connection */

    fflush(stdout);
    fflush(stderr);

    while(fe->fe_run(fe) && !fe->xine_is_finished(fe,0) && !terminate_key_pressed) 
      pthread_yield();

    fe->xine_close(fe);
    firsttry = 0;

  } while(!terminate_key_pressed && reconnect);

  /* Clean up */

  printf("Terminating...\n");

  lirc_stop();
  if(!nokbd) kbd_stop();

  fe->fe_free(fe); 
  return terminate_key_pressed ? 0 : 1;
}
