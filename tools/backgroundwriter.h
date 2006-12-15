/*
 * backgroundwriter.h: Buffered socket/file writing thread
 *
 * See the main source file 'xineliboutput.c' for copyright information and
 * how to reach the author.
 *
 * $Id: backgroundwriter.h,v 1.3 2006-12-15 16:35:31 phintuka Exp $
 *
 */

#ifndef __BACKGROUNDWRITER_H
#define __BACKGROUNDWRITER_H

#include <stdint.h>

#include <vdr/thread.h>
#include <vdr/ringbuffer.h>


class cBackgroundWriter : public cThread {

  private:
    cRingBufferLinear m_RingBuffer;

    volatile bool m_Active;
    bool m_Raw; /* stream without stream_tcp_header_t */
    int  m_fd;

    uint64_t m_PutPos;
    uint64_t m_DiscardStart;
    uint64_t m_DiscardEnd;

    int m_BufferOverflows;

  protected:
    virtual void Action(void);

    int Put(const uchar *Header, int HeaderCount, 
	    const uchar *Data,   int DataCount);

  public:
    cBackgroundWriter(int fd, int Size = KILOBYTE(512), bool Raw = false);
    virtual ~cBackgroundWriter() ;

    // Return largest possible Put size
    int Free(void);

    // Add PES frame to buffer
    //
    // Return value:
    // Success:     Count   (all bytes pushed to queue)
    // Error:       0       (write error ; socket disconnected)
    // Buffer full: -Count  (no bytes will be pushed to queue)
    //
    int Put(uint64_t StreamPos, const uchar *Data, int DataCount);

    // Drop all data (only complete frames) from buffer
    void Clear(void);

    bool Flush(int TimeoutMs);
};


#endif
