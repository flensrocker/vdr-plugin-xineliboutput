/*
 * cxsocket.h: Socket wrapper classes
 *
 * See the main source file 'xineliboutput.c' for copyright information and
 * how to reach the author.
 *
 * $Id: cxsocket.h,v 1.5 2006-07-07 05:28:04 phintuka Exp $
 *
 */

#ifndef __CXSOCKET_H
#define __CXSOCKET_H

#define CLOSESOCKET(fd) do { if(fd>=0) { close(fd); fd=-1; } } while(0)

//
// Set socket buffers
//
static inline void set_socket_buffers(int s, int txbuf, int rxbuf)
{
  int max_buf = txbuf;
  /*while(max_buf) {*/
    errno = 0;
    if(setsockopt(s, SOL_SOCKET, SO_SNDBUF, &max_buf, sizeof(int))) {
      LOGERR("setsockopt(SO_SNDBUF,%d) failed", max_buf);
      /*max_buf >>= 1;*/
    }
    /*else {*/
      int tmp = 0;
      int len = sizeof(int);
      errno = 0;
      if(getsockopt(s, SOL_SOCKET, SO_SNDBUF, &tmp, (socklen_t*)&len)) {
	LOGERR("getsockopt(SO_SNDBUF,%d) failed", max_buf);
	/*break;*/
      } else if(tmp != max_buf) {
	LOGDBG("setsockopt(SO_SNDBUF): got %d bytes", tmp);
	/*max_buf >>= 1;*/
	/*continue;*/
      }
    /*}*/
  /*}*/

  max_buf = rxbuf;
  setsockopt(s, SOL_SOCKET, SO_RCVBUF, &max_buf, sizeof(int));
}

//
// Set multicast options
//
static inline int set_multicast_options(int fd_multicast, int ttl)
{
  int iReuse = 1, iLoop = 1, iTtl = xc.remote_rtp_ttl;

  errno = 0;

  if(setsockopt(fd_multicast, SOL_SOCKET, SO_REUSEADDR, 
		&iReuse, sizeof(int)) < 0) {
    LOGERR("setsockopt(SO_REUSEADDR) failed");
    return -1;
  }  

  if(setsockopt(fd_multicast, IPPROTO_IP, IP_MULTICAST_TTL, 
		&iTtl, sizeof(int))) {
    LOGERR("setsockopt(IP_MULTICAST_TTL) failed");
    return -1;
  }
      
  if(setsockopt(fd_multicast, IPPROTO_IP, IP_MULTICAST_LOOP,
		&iLoop, sizeof(int))) {
    LOGERR("setsockopt(IP_MULTICAST_LOOP) failed");
    return -1;
  }

  return 0;
}

//
// Connect data socket to client (take address from fd_control)
//
static inline int sock_connect(int fd_control, int port, int type)
{
  struct sockaddr_in sin;
  socklen_t len = sizeof(sin);
  int             s;

  if(getpeername(fd_control, (struct sockaddr *)&sin, &len)) {
    LOGERR("sock_connect: getpeername failed");
    return -1;
  }

  uint32_t tmp = ntohl(sin.sin_addr.s_addr);
  LOGMSG("Client address: %d.%d.%d.%d", 
	 ((tmp>>24)&0xff), ((tmp>>16)&0xff), 
	 ((tmp>>8)&0xff), ((tmp)&0xff));

#if 0
  if ((h = gethostbyname(tmp)) == NULL) {
    LOGDBG("sock_connect: unable to resolve host name", tmp);
  }
#endif

  if ((s = socket(PF_INET, type, 
		  type==SOCK_DGRAM?IPPROTO_UDP:IPPROTO_TCP)) < 0) {
    LOGERR("sock_connect: failed to create socket");
    return -1;
  }

  // Set socket buffers: large send buffer, small receive buffer
  set_socket_buffers(s, KILOBYTE(256), 2048);

  sin.sin_family = AF_INET;
  sin.sin_port   = htons(port);
  
  if (connect(s, (struct sockaddr *)&sin, sizeof(sin))==-1 && 
      errno != EINPROGRESS) {
    LOGERR("connect() failed");
    CLOSESOCKET(s);
  }

  if (fcntl (s, F_SETFL, fcntl (s, F_GETFL) | O_NONBLOCK) == -1) {
    LOGERR("can't put socket in non-blocking mode");
    CLOSESOCKET(s);
    return -1;
  }

  return s;
}

static inline ssize_t timed_write(int fd, const void *buffer, size_t size, 
				  int timeout_ms)
{
  ssize_t written = (ssize_t)size;
  const unsigned char *ptr = (const unsigned char *)buffer;
  cPoller poller(fd, true);

  while (size > 0) {

    if(!poller.Poll(timeout_ms)) {
      LOGERR("timed_write: poll() failed");
      return written-size;
    }

    errno = 0;
    ssize_t p = write(fd, ptr, size);

    if (p <= 0) {
      if (errno == EINTR || errno == EAGAIN) {
	LOGDBG("timed_write: EINTR during write(), retrying");
	continue;
      }
      LOGERR("timed_write: write() error");
      return p;
    }

    ptr  += p;
    size -= p;    
  }

  return written;
}

//#include "xine_osd_command.h"

static inline int write_osd_command(int fd, osd_command_t *cmd)
{
  if(8 != timed_write(fd, "OSDCMD\r\n", 8, 200)) {
    LOGDBG("write_osd_command: write (command) failed");
    return 0;
  }
  if((ssize_t)sizeof(osd_command_t) != 
     timed_write(fd, cmd, sizeof(osd_command_t), 200)) {
    LOGDBG("write_osd_command: write (data) failed");
    return 0;
  }
  if(cmd->palette && cmd->colors &&
     (ssize_t)(sizeof(xine_clut_t)*ntohl(cmd->colors)) != 
     timed_write(fd, cmd->palette, sizeof(xine_clut_t)*ntohl(cmd->colors), 200)) {
    LOGDBG("write_osd_command: write (palette) failed");
    return 0;
  }
  if(cmd->data && cmd->datalen &&
     (ssize_t)ntohl(cmd->datalen) != timed_write(fd, cmd->data, ntohl(cmd->datalen), 1000)) {
    LOGDBG("write_osd_command: write (bitmap) failed");
    return 0;
  }
  return 1;
}

static inline ssize_t write_str(int fd, const char *str, int timeout_ms=-1)
{
  return timed_write(fd, str, strlen(str), timeout_ms);
}

static inline ssize_t write_cmd(int fd, const char *str)
{
  return write_str(fd, str, 10);
}

static inline int udp_discovery_broadcast(int fd_discovery, int m_Port)
{
  if(!xc.remote_usebcast) {
    LOGDBG("UDP broadcasts (discovery) disabled in configuration");
    return -1;
  }

  struct sockaddr_in sin;

  sin.sin_family = AF_INET;
  sin.sin_port   = htons(DISCOVERY_PORT);
  sin.sin_addr.s_addr = INADDR_BROADCAST;
  
  char *test = NULL;
  asprintf(&test, 
	   "VDR xineliboutput DISCOVERY 1.0\r\n"
	   "Server port: %d\r\n"
	   "Server version: vdr-" VDRVERSION "\r\n"
	   "\txineliboutput-" XINELIBOUTPUT_VERSION "\r\n"
	   "\r\n",
	   m_Port);
  int testlen = strlen(test);
  if(testlen != sendto(fd_discovery, test, testlen, 0,
		       (struct sockaddr *)&sin, sizeof(sin))) {
    LOGERR("UDP broadcast send failed (discovery)");
    return -1;
  } else {
    LOGDBG("UDP broadcast send succeed (discovery)");
  }
  return 1;
}


#endif // __CXSOCKET_H
