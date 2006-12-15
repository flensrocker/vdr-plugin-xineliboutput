/*
 * backgroundwriter.h: Buffered socket/file writing thread
 *
 * See the main source file 'xineliboutput.c' for copyright information and
 * how to reach the author.
 *
 * $Id: backgroundwriter.c,v 1.3 2006-12-15 16:35:31 phintuka Exp $
 *
 */

#include <stdint.h>
#include <unistd.h>

#include <vdr/tools.h>

#include "../logdefs.h"
#include "../xine_input_vdr_net.h" // stream_tcp_header_t

#include "backgroundwriter.h"

//#define DISABLE_DISCARD
//#define LOG_DISCARDS

#define MAX_OVERFLOWS_BEFORE_DISCONNECT 500 // ~ 1 second

cBackgroundWriter::cBackgroundWriter(int fd, int Size, bool Raw) 
  : m_RingBuffer(Size, sizeof(stream_tcp_header_t))
{ 
  m_fd = fd; 
  m_RingBuffer.SetTimeouts(0, 100);
  m_Active = true;
  m_Raw = Raw;

  m_PutPos = 0;
  m_DiscardStart = 0;
  m_DiscardEnd   = 0;

  m_BufferOverflows = 0;

  LOGDBG("cBackgroundWriter%s initialized (buffer %d kb)", 
	 Raw?"(RAW)":"", Size/1024);

  Start();
}

cBackgroundWriter::~cBackgroundWriter() 
{
  m_Active = false;
  Cancel(3);
}

int cBackgroundWriter::Free(void) 
{
  return m_RingBuffer.Free();
}

void cBackgroundWriter::Action(void)
{
  uint64_t NextHeaderPos = 0ULL;
  uint64_t GetPos = 0ULL;
  cPoller Poller(m_fd, true);

  while(m_Active) {

    if(Poller.Poll(100)) {

      int Count = 0, n;
      uchar *Data = m_RingBuffer.Get(Count);

      if(Data && Count > 0) {

#ifndef DISABLE_DISCARD
	Lock(); // uint64_t m_DiscardStart can not be read atomically (IA32)
	if(m_DiscardEnd > GetPos) {

# ifdef LOG_DISCARDS
	  LOGMSG("TCP: queue: discard request: queue %d bytes, "
		 "next point %d bytes forward (Count=%d)",
		 m_RingBuffer.Available(),
		 NextHeaderPos - GetPos, 
		 Count);
# endif
	  if(NextHeaderPos == GetPos) {
	    // we're at frame boundary
# ifdef LOG_DISCARDS
	    uint8_t *pkt = TCP_PAYLOAD(Data);
	    if(pkt[0] || pkt[1] || pkt[2] != 1 || hdr->len > 2100) {
	      LOGMSG("  -> %x %x %x %x", pkt[0], pkt[1], pkt[2], pkt[3]);
	    }      
# endif
	    Count = min(Count, (int)(m_DiscardEnd - GetPos));
# ifdef LOG_DISCARDS
	    LOGMSG("Flushing %d bytes", Count);
#endif
	    Unlock();

	    m_RingBuffer.Del(Count);
	    GetPos += Count;
	    NextHeaderPos = GetPos;
# ifdef LOG_DISCARDS
	    LOGMSG("Queue now %d bytes", m_RingBuffer.Available());
	    pkt = TCP_PAYLOAD(Data);
	    if(pkt[0] || pkt[1] || pkt[2] != 1 || hdr->len > 2100) {
	      LOGMSG("  -> %x %x %x %x", pkt[0], pkt[1], pkt[2], pkt[3]);
# endif
	    continue;
	  }
	}
	Unlock();
#endif

#ifndef DISABLE_DISCARD
	if(GetPos == NextHeaderPos) {
	  if(Count < (int)sizeof(stream_tcp_header_t))
	    LOGMSG("cBackgroundWriter @NextHeaderPos: Count < header size !");

	  stream_tcp_header_t *header = (stream_tcp_header_t*)Data;
	  if(Count < (int)(ntohl(header->len) + sizeof(stream_tcp_header_t)))
	    ;//LOGMSG("Count = %d < %d", Count, 
	     //   header->len + sizeof(stream_tcp_header_t));
	  else
	    Count = ntohl(header->len) + sizeof(stream_tcp_header_t);
	  NextHeaderPos = GetPos + ntohl(header->len) + sizeof(stream_tcp_header_t);
	} else {
	  Count = min(Count, (int)(NextHeaderPos-GetPos));
	}
#endif

	errno = 0;
	if(!m_Raw)
	  n = write(m_fd, Data, Count);
	else
	  n = write(m_fd, 
		    Data + sizeof(stream_tcp_header_t), 
		    Count - sizeof(stream_tcp_header_t));

	if(n == 0) {
	  LOGERR("cBackgroundWriter: Client disconnected data stream ?");
	  break;
	  
	} else if(n < 0) {
	  
	  if (errno == EINTR || errno == EWOULDBLOCK) {
	    TRACE("cBackgroundWriter: EINTR while writing to file handle "
		  <<m_fd<<" - retrying");
	    continue;

	  } else {
	    LOGERR("cBackgroundWriter: TCP write error");
	    break;
	  }
	}

	if(m_Raw)
	  n += sizeof(stream_tcp_header_t);

	GetPos += n;
	m_RingBuffer.Del(n);
      }
    }
  }

  m_RingBuffer.Clear();
  m_Active = false;
}

void cBackgroundWriter::Clear(void) 
{
  // Can't just drop buffer contents or PES frames will be broken.

  // Serialize with Put
  LOCK_THREAD; 
#ifdef LOG_DISCARDS
  LOGMSG("cBackgroundWriter::Clear() @%lld", m_PutPos);
#endif
  m_DiscardEnd = m_PutPos;
}

bool cBackgroundWriter::Flush(int TimeoutMs)
{
  uint64_t WaitEnd = cTimeMs::Now();

  if(TimeoutMs > 0)
    WaitEnd += (uint64_t)TimeoutMs;

  while(cTimeMs::Now() < WaitEnd &&
	m_Active &&
	m_RingBuffer.Available() > 0)
    cCondWait::SleepMs(3);
  
  return m_RingBuffer.Available() <= 0;
}

int cBackgroundWriter::Put(uint64_t StreamPos, 
			   const uchar *Data, int DataCount)
{
  stream_tcp_header_t header;
  header.pos = htonull(StreamPos);
  header.len = htonl(DataCount);
  return Put((uchar*)&header, sizeof(header), Data, DataCount);
}

int cBackgroundWriter::Put(const uchar *Header, int HeaderCount, 
			   const uchar *Data, int DataCount) 
{
  if(m_Active) {

    // Serialize Put access to keep Data and Header together
    LOCK_THREAD;  
    
    if(m_RingBuffer.Free() < HeaderCount+DataCount) {
      //LOGMSG("cXinelibServer: TCP buffer overflow !");
      if(m_BufferOverflows++ > MAX_OVERFLOWS_BEFORE_DISCONNECT) {
	LOGMSG("cXinelibServer: Too many TCP buffer overflows, dropping client");
	m_RingBuffer.Clear();
	m_Active = false;
	return 0;
      }
      return -HeaderCount-DataCount;
    }
    int n = m_RingBuffer.Put(Header, HeaderCount) +
            m_RingBuffer.Put(Data, DataCount);
    if(n == HeaderCount+DataCount) {
      m_BufferOverflows = 0;
      m_PutPos += n;
      return n;
    }
    
    LOGMSG("cXinelibServer: TCP buffer internal error ?!?");
    m_RingBuffer.Clear();
    m_Active = false;
  }

  return 0;
}
