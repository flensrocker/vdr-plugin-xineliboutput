/*
 * cxsocket.h: Socket wrapper classes
 *
 * See the main source file 'xineliboutput.c' for copyright information and
 * how to reach the author.
 *
 * $Id: cxsocket.h,v 1.16 2006-12-31 20:19:32 phintuka Exp $
 *
 */

#ifndef __CXSOCKET_H
#define __CXSOCKET_H

#include <inttypes.h>
#include <sys/types.h>
#include <sys/socket.h>

#define CLOSESOCKET(fd) do { if(fd>=0) { close(fd); fd=-1; } } while(0)

class cxSocket {
 private:
  int m_fd;

 public:

  typedef enum {
    estSTREAM = SOCK_STREAM,
    estDGRAM  = SOCK_DGRAM
  } eSockType;

  cxSocket(int fd = -1) : m_fd(fd) {}
  cxSocket(eSockType type) : m_fd(::socket(PF_INET, (int)type, 0)) {}

  cxSocket(const cxSocket& s) : m_fd(s.m_fd) {}
  cxSocket &operator=(const cxSocket &S) { m_fd = S.m_fd; return *this; };

  operator int ()  const { return Handle(); }
  operator bool () const { return IsOpen(); }

  int  Handle(void) const { return m_fd; }
  bool IsOpen(void) const { return m_fd>0; }
  void Close(void)        { CLOSESOCKET(m_fd); }

  ssize_t sendto(const void *buf, size_t size, 
		 const struct sockaddr *to, socklen_t tolen);
  ssize_t recvfrom(void *buf, size_t size,
		   struct sockaddr *from, socklen_t *fromlen);
  ssize_t read(void *buffer, size_t size, int timeout_ms = -1);
  ssize_t write(const void *buffer, size_t size, int timeout_ms = -1);

  ssize_t printf(const char *fmt, ...) __attribute__ ((format (printf, 2, 3)));
  ssize_t write_str(int fd, const char *str, int timeout_ms=-1, int len=0)
  { return write(str, len ?: strlen(str), timeout_ms); }
  ssize_t write_cmd(int fd, const char *str, int len=0)
  { return write(str, len ?: strlen(str), 10); }

/* readline return value:
 *   <0        : failed
 *   >=maxsize : buffer overflow
 *   >=0       : if errno = EAGAIN -> line is not complete (there was timeout)
 *               if errno = 0      -> succeed
 *               (return value 0 indicates empty line "\r\n")
 */
  ssize_t readline(char *buf, int bufsize, int timeout=0, int bufpos=0);

  bool SetBuffers(int Tx, int Rx);
  bool SetMulticast(int ttl);

  static char *ip2txt(uint32_t ip, unsigned int port, char *str);
};

#include <vdr/tools.h>

class cxPoller : public cPoller {
  public:
    cxPoller(cxSocket& Sock, bool Out=false) : cPoller(Sock, Out) {};

    cxPoller(cxSocket* Socks, int count, bool Out=false) 
    {
      for(int i=0; i<count; i++)
	Add(Socks[i], Out);
    }
};


//#warning TODO: use cxsocket and remove all other printf_cmd, write_cmd, ....

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
  int iReuse = 1, iLoop = 1, iTtl = ttl;

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
  int             s, one = 1;

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

  if(setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(int)) < 0)
    LOGERR("sock_connect: setsockopt(SO_REUSEADDR) failed");

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
    errno = 0;
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

static inline ssize_t timed_read(int fd, void *buffer, size_t size, 
				 int timeout_ms)
{
  ssize_t missing = (ssize_t)size;
  unsigned char *ptr = (unsigned char *)buffer;
  cPoller poller(fd);

  while (missing > 0) {

    if(!poller.Poll(timeout_ms)) {
      LOGERR("timed_read: poll() failed at %d/%d", (int)(size-missing), (int)size);
      return size-missing;
    }

    errno = 0;
    ssize_t p = read(fd, ptr, missing);

    if (p <= 0) {
      if (errno == EINTR || errno == EAGAIN) {
	LOGDBG("timed_read: EINTR/EAGAIN during read(), retrying");
	continue;
      }
      LOGERR("timed_read: read() error at %d/%d", (int)(size-missing), (int)size);
      return size-missing;
    }

    ptr  += p;
    missing -= p;    
  }

  return size;
}

#include "../xine_osd_command.h"

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

static inline ssize_t write_str(int fd, const char *str, int timeout_ms=-1, int len=0)
{
  return timed_write(fd, str, len ? : strlen(str), timeout_ms);
}

static inline ssize_t write_cmd(int fd, const char *str, int len=0)
{
  return write_str(fd, str, 10, len);
}

#include <stdarg.h>

static inline ssize_t printf_cmd(int fd, const char *fmt, ...) __attribute__ ((format (printf, 2, 3)));

static inline ssize_t printf_cmd(int fd, const char *fmt, ...)
{
  va_list argp;
  char buf[256];
  int r;

  va_start(argp, fmt);
  r = vsnprintf(buf, sizeof(buf), fmt, argp);
  if(r<0)
    LOGERR("printf_cmd: vsnprintf failed");
  else if(r >= (int)sizeof(buf))
    LOGMSG("printf_cmd: vsnprintf overflow (%20s)", buf);
  else
    return write_cmd(fd, buf, r);

  return (ssize_t)-1;
}

#include "../config.h"
#include "../xine_input_vdr_net.h"
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
	   DISCOVERY_1_0_HDR     //"VDR xineliboutput DISCOVERY 1.0" "\r\n"
	   DISCOVERY_1_0_SVR     //"Server port: %d" "\r\n"
	   DISCOVERY_1_0_VERSION //"Server version: vdr-" VDRVERSION "\r\n"
                                 //"\txineliboutput-" XINELIBOUTPUT_VERSION "\r\n"
	   "\r\n",
	   m_Port);

  int testlen = strlen(test);
  int result;
  if(testlen != sendto(fd_discovery, test, testlen, 0,
		       (struct sockaddr *)&sin, sizeof(sin))) {
    LOGERR("UDP broadcast send failed (discovery)");
    result = -1;
  } else {
    //LOGDBG("UDP broadcast send succeed (discovery)");
    result = 1;
  }
  free(test);
  return result;
}


#endif // __CXSOCKET_H
