/*
 * cxsocket.c: Socket wrapper classes
 *
 * See the main source file 'xineliboutput.c' for copyright information and
 * how to reach the author.
 *
 * $Id: cxsocket.c,v 1.3 2006-12-31 20:29:22 phintuka Exp $
 *
 */

#define __STDC_FORMAT_MACROS
#include <inttypes.h>

#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <vdr/config.h>
#include <vdr/tools.h>

#include "../logdefs.h"

#include "cxsocket.h"

bool cxSocket::SetBuffers(int Tx, int Rx)
{
  int max_buf = Tx;
  /*while(max_buf) {*/
    errno = 0;
    if(setsockopt(m_fd, SOL_SOCKET, SO_SNDBUF, &max_buf, sizeof(int))) {
      LOGERR("cxSocket: setsockopt(SO_SNDBUF,%d) failed", max_buf);
      /*max_buf >>= 1;*/
    }
    /*else {*/
      int tmp = 0;
      int len = sizeof(int);
      errno = 0;
      if(getsockopt(m_fd, SOL_SOCKET, SO_SNDBUF, &tmp, (socklen_t*)&len)) {
	LOGERR("cxSocket: getsockopt(SO_SNDBUF,%d) failed", max_buf);
	/*break;*/
      } else if(tmp != max_buf) {
	LOGDBG("cxSocket: setsockopt(SO_SNDBUF): got %d bytes", tmp);
	/*max_buf >>= 1;*/
	/*continue;*/
      }
    /*}*/
  /*}*/

  max_buf = Rx;
  setsockopt(m_fd, SOL_SOCKET, SO_RCVBUF, &max_buf, sizeof(int));

  return true;
}

bool cxSocket::SetMulticast(int ttl)
{
  int iReuse = 1, iLoop = 1, iTtl = ttl;

  errno = 0;

  if(setsockopt(m_fd, SOL_SOCKET, SO_REUSEADDR, 
		&iReuse, sizeof(int)) < 0) {
    LOGERR("cxSocket: setsockopt(SO_REUSEADDR) failed");
    return false;
  }  

  if(setsockopt(m_fd, IPPROTO_IP, IP_MULTICAST_TTL, 
		&iTtl, sizeof(int))) {
    LOGERR("cxSocket: setsockopt(IP_MULTICAST_TTL) failed");
    return false;
  }
      
  if(setsockopt(m_fd, IPPROTO_IP, IP_MULTICAST_LOOP,
		&iLoop, sizeof(int))) {
    LOGERR("cxSocket: setsockopt(IP_MULTICAST_LOOP) failed");
    return false;
  }

  return true;
}

ssize_t cxSocket::sendto(const void *buf, size_t size, 
			 const struct sockaddr *to, socklen_t tolen)
{
  LOGMSG("cxSocket::sendto not implemented !");
  return -1;
}

ssize_t cxSocket::recvfrom(void *buf, size_t size,
			   struct sockaddr *from, socklen_t *fromlen)
{
  LOGMSG("cxSocket::recvfrom not implemented !");
  return -1;
}

ssize_t cxSocket::write(const void *buffer, size_t size, 
			      int timeout_ms)
{
  ssize_t written = (ssize_t)size;
  const unsigned char *ptr = (const unsigned char *)buffer;
  cPoller poller(m_fd, true);

  while (size > 0) {
    errno = 0;
    if(!poller.Poll(timeout_ms)) {
      LOGERR("cxSocket::write: poll() failed");
      return written-size;
    }

    errno = 0;
    ssize_t p = ::write(m_fd, ptr, size);

    if (p <= 0) {
      if (errno == EINTR || errno == EAGAIN) {
	LOGDBG("cxSocket::write: EINTR during write(), retrying");
	continue;
      }
      LOGERR("cxSocket::write: write() error");
      return p;
    }

    ptr  += p;
    size -= p;    
  }

  return written;
}

ssize_t cxSocket::read(void *buffer, size_t size, int timeout_ms)
{
  ssize_t missing = (ssize_t)size;
  unsigned char *ptr = (unsigned char *)buffer;
  cPoller poller(m_fd);

  while (missing > 0) {

    if(!poller.Poll(timeout_ms)) {
      LOGERR("cxSocket::read: poll() failed at %d/%d", (int)(size-missing), (int)size);
      return size-missing;
    }

    errno = 0;
    ssize_t p = ::read(m_fd, ptr, missing);

    if (p <= 0) {
      if (errno == EINTR || errno == EAGAIN) {
	LOGDBG("cxSocket::read: EINTR/EAGAIN during read(), retrying");
	continue;
      }
      LOGERR("cxSocket::read: read() error at %d/%d", (int)(size-missing), (int)size);
      return size-missing;
    }

    ptr  += p;
    missing -= p;    
  }

  return size;
}

ssize_t cxSocket::printf(const char *fmt, ...)
{
  va_list argp;
  char buf[256];
  int r;

  va_start(argp, fmt);
  r = vsnprintf(buf, sizeof(buf), fmt, argp);
  if(r<0)
    LOGERR("cxSocket::printf: vsnprintf failed");
  else if(r >= (int)sizeof(buf))
    LOGMSG("cxSocket::printf: vsnprintf overflow (%20s)", buf);
  else
    return write(buf, r);
  
  return (ssize_t)-1;
}

/* readline return value:
 *   <0        : failed
 *   >=maxsize : buffer overflow
 *   >=0       : if errno = EAGAIN -> line is not complete (there was timeout)
 *               if errno = 0      -> succeed
 *               (return value 0 indicates empty line "\r\n")
 */
ssize_t cxSocket::readline(char *buf, int bufsize, int timeout, int bufpos)
{
  int n = -1, cnt = bufpos;
  cPoller p(m_fd);

  do {
    if(timeout>0 && !p.Poll(timeout)) {
      errno = EAGAIN;
      return cnt;
    }

    while((n = ::read(m_fd, buf+cnt, 1)) == 1) {
      buf[++cnt] = 0;

      if( cnt > 1 && buf[cnt - 2] == '\r' && buf[cnt - 1] == '\n') {
	cnt -= 2;
	buf[cnt] = 0;
	errno = 0;
	return cnt;
      }

      if( cnt >= bufsize) {
	LOGMSG("cxSocket::readline: too long control message (%d bytes): %20s", cnt, buf);
	errno = 0;
	return bufsize;
      }
    }

    /* connection closed ? */
    if (n == 0) {
      LOGMSG("cxSocket::readline: disconnected");
      if(errno == EAGAIN)
	errno = ENOTCONN;
      return -1;
    }

  } while (timeout>0 && n<0 && errno == EAGAIN);

  if(errno == EAGAIN)
    return cnt;

  LOGERR("cxSocket::readline: read failed");
  return n;
}

char *cxSocket::ip2txt(uint32_t ip, unsigned int port, char *str)
{
  // inet_ntoa is not thread-safe (?)
  if(str) {
    unsigned int iph =(unsigned int)ntohl(ip);
    unsigned int porth =(unsigned int)ntohs(port);
    if(!porth)
      sprintf(str, "%d.%d.%d.%d", 
	      ((iph>>24)&0xff), ((iph>>16)&0xff), 
	      ((iph>>8)&0xff), ((iph)&0xff));
    else
      sprintf(str, "%u.%u.%u.%u:%u", 
	      ((iph>>24)&0xff), ((iph>>16)&0xff), 
	      ((iph>>8)&0xff), ((iph)&0xff),
	      porth);
  }
  return str;
}
