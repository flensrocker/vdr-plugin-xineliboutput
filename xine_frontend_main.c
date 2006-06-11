/*
 * Simple main() routine for stand-alone frontends.
 *
 * See the main source file 'xineliboutput.c' for copyright information and
 * how to reach the author.
 *
 * $Id: xine_frontend_main.c,v 1.4 2006-06-11 19:08:05 phintuka Exp $
 *
 */

#include <termios.h>
#include <unistd.h>

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
  /* from vdr, remote.c */
  struct pollfd pfd;
  pfd.fd = STDIN_FILENO;
  pfd.events = POLLIN;
  if(poll(&pfd, 1, 50) == 1) {
    unsigned char ch = 0;
    int r = read(STDIN_FILENO, &ch, 1);
    if (r == 1)
      return (int)ch;
    if (r < 0) {
      LOGERR("read_key: read failed");
      return -2;
    }
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

int main(int argc, char *argv[])
{
  char *mrl = NULL, *gdrv = NULL, *adrv = NULL, *adev = NULL;
  int ftcp = 0, fudp = 0, frtp = 0;
  int fullscreen = 0, width = 720, height = 576;
  int scale_video = 1, aspect = 1;
  char *video_port = NULL;
  int xmajor, xminor, xsub;
  int i, err;
  frontend_t *fe = NULL;
  extern const fe_creator_f fe_creator;
  char *static_post_plugins = NULL;
  void *p;
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
  for(i=1; i<argc; i++) {
    if(!strncmp(argv[i], "--help", 6)) {
      printf("\n"
	     "Usage: %s [options] [xvdr:[udp:|tcp:|rtp:]//host:port] \n"
	     "\n"
	     "Available options:\n"
	     "   --help                       \n"
	     "   --fullscreen                 \n"
	     "   --width x                    \n"
	     "   --height x                   \n"
	     "   --lirc [devicename]          \n"
	     "   --audio audiodriver[:device] \n"
	     "   --video videodriver          \n", 
	     exec_name);
      printf("   --display displayaddress     \n"
	     "   --verbose                    \n"
	     "   --silent                     \n"
	     "   --syslog                     \n"
	     "   --tcp                        \n"
	     "   --udp                        \n"
	     "   --rtp                        \n"
	     "   --aspect [auto|4:3|16:9|default] \n"
	     "   --noscaling                  \n"
	     "   --post name[:arg=val[,arg=val]]\n");
      exit(0);
    } else if(!strncmp(argv[i], "--fullscreen", 12)) {
      fullscreen=1;
      printf("Fullscreen mode\n");
    } else if(!strncmp(argv[i], "--verbose", 9)) {
      verbose_xine_log = 1;
      SysLogLevel = 3;
      printf("Verbose mode\n");
    } else if(!strncmp(argv[i], "--silent", 8)) {
      verbose_xine_log = 0;
      SysLogLevel = 1;
      printf("Silent mode\n");
    } else if(!strncmp(argv[i], "--noscaling", 11)) {
      scale_video = 0;
      printf("Video scaling disabled\n");
    } else if(!strncmp(argv[i], "--tcp", 5)) {
      ftcp = 1;
      printf("Protocol: TCP\n");
    } else if(!strncmp(argv[i], "--udp", 5)) {
      fudp = 1;
      printf("Protocol: UDP\n");
    } else if(!strncmp(argv[i], "--rtp", 5)) {
      frtp = 1;
      printf("Protocol: RTP\n");
    } else if(!strncmp(argv[i], "--syslog", 8)) {
      LogToSysLog = 1;
      openlog(exec_name, LOG_PID|LOG_CONS, LOG_USER);
    } else if(!strncmp(argv[i], "--video", 7)) {
      if(argc > ++i) { 
	gdrv = strdup(argv[i]);
	printf("Video driver: %s\n",gdrv);
      }
    } else if(!strncmp(argv[i], "--audio", 7)) {
      if(argc > ++i) {
	adrv = strdup(argv[i]);
	adev = strchr(adrv, ':');
	if(adev)
	  *(adev++) = 0;
	printf("Audio driver: %s\n",adrv);
	if(adev)
	  printf("Audio device: %s\n",adev);
      }
    } else if(!strncmp(argv[i], "--post", 6)) {
      if(argc > ++i) {
	if(static_post_plugins)
	  strcatrealloc(static_post_plugins, ";");
	static_post_plugins = strcatrealloc(static_post_plugins, argv[i]);
	printf("Post plugins: %s\n", static_post_plugins);
      }	
    } else if(!strncmp(argv[i], "--height", 8)) {
      if(argc > ++i) height = atoi(argv[i]);
      printf("Height: %d\n", height);
    } else if(!strncmp(argv[i], "--aspect", 8)) {
      if(argc > ++i) {
	if(!strncmp(argv[i], "auto", 4))
	  aspect = 0;
	if(!strncmp(argv[i], "4:3", 3))
	  aspect = 2;
	if(!strncmp(argv[i], "16:9", 4))
	  aspect = 3;
	printf("Aspect ratio: %s\n", 
	       aspect==0?"Auto":aspect==2?"4:3":aspect==3?"16:9":"Default");
      }
    } else if(!strncmp(argv[i], "--display", 9)) {
      if(argc > ++i) video_port = strdup(argv[i]);
    } else if(!strncmp(argv[i], "--width", 7)) {
      if(argc > ++i) width = atoi(argv[i]);
    } else if(!strncmp(argv[i], "--lirc", 6)) {
      if(argc > i+1 && argv[i+1][0] == '/') 
	lirc_device_name = strdup(argv[++i]);
      else
	lirc_device_name = strdup("/dev/lircd");
      printf("LIRC device:  %s\n", lirc_device_name);
    } else {
      if(argv[i][0] != '-') {
	mrl = strdup(argv[i]);
	printf("VDR Server: %s\n", mrl);
      } else {
	fprintf(stderr, "Unknown argument: %s\n", argv[i]);
	exit(-1);
      }
    }
  }
  printf("\n");

  /* check xine-lib version */
  if(!xine_check_version(1, 1, 0)) {
    fprintf(stderr,"ERROR: xine-lib is too old, require at least "
	    "xine library version 1.1.0\n");
    return 1;
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
    if(search_vdr_server(&port, &address[0])) {
      printf("Found VDR server: host %s, port %d\n", address, port);
      if(mrl) {
	char *tmp = mrl;
	mrl = NULL;
	asprintf(&mrl, "%s//%s:%d", tmp, address, port);
	free(tmp);
      } else
	asprintf(&mrl, "xvdr://%s:%d", address, port);
    } else {
      printf("WARNING:\n"
	     "   MRL not given and server not found from local network.\n"
	     "   Trying to connect to default port on local host.\n");
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

  /* Create front-end */
  fe = (*fe_creator)();
  if(!fe) {
    printf("Error initializing frontend\n");
    return 3;
  }

  /* Initialize display */
  if(!fe->fe_display_open(fe, width, height, fullscreen, 0, 
			  "", aspect, NULL, video_port, scale_video, 0)) {
    printf("Error opening display\n");
    fe->fe_free(fe);
    return 4;
  }

  /* Initialize xine */
  if(!fe->xine_init(fe, adrv, adev, gdrv, 250, 1, static_post_plugins)) {
    printf("Error initializing xine\n");
    fe->fe_free(fe);
    return 5;
  }

  /* Connect to VDR xineliboutput server */
  if(!fe_xine_open(fe, mrl)) {
    /*print_xine_log(((fe_t *)fe)->xine);*/
    printf("Error opening %s\n", mrl);
    fe->fe_free(fe);
    return 6;
  }

  printf("\n\nPress Esc to exit\n\n");

  if(!fe->xine_play(fe)) {
    /*print_xine_log(((fe_t *)fe)->xine);*/
    /*printf("Error playing %s//%s:%s\n", argv[1], host, port);*/
    printf("Error playing %s\n", argv[1]);
    fe->fe_free(fe);
    return 7;
  }

  /* Start LIRC forwarding */
  if(lirc_device_name) {
    if ((err = pthread_create (&lirc_thread,
			       NULL, lirc_receiver_thread, 
			       (void*)fe)) != 0) {
      fprintf(stderr, "can't create new thread for lirc (%s)\n", 
	      strerror(err));
    }
  }

  /* Start keyboard listener thread */
  if ((err = pthread_create (&kbd_thread,
			     NULL, kbd_receiver_thread, 
			     (void*)fe)) != 0) {
    fprintf(stderr, "can't create new thread for lirc (%s)\n", 
	    strerror(err));
  }
   
  /* Main loop */

  sleep(2);  /* give input_vdr some time to establish connection */

  while(fe->fe_run(fe) && !fe->xine_is_finished(fe) && !terminate_key_pressed) 
    pthread_yield();

  /* Clean up */

  printf("Terminating...\n");

  if(lirc_device_name) {
    /*free(lirc_device_name);*/    
    lirc_device_name = NULL;
    if(fd_lirc >= 0)
      close(fd_lirc);
    fd_lirc = -1;
    pthread_cancel (lirc_thread);
    pthread_join (lirc_thread, &p);
  }

  pthread_cancel (kbd_thread);
  pthread_join (kbd_thread, &p);

  tcsetattr(STDIN_FILENO, TCSANOW, &saved_tm);

  fe->fe_free(fe); 
  return 0;
}
