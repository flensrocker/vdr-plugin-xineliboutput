/*
 * udp_buffer.h: Ring buffer for UDP/RTP streams
 *
 * See the main source file 'xineliboutput.c' for copyright information and
 * how to reach the author.
 *
 * $Id: udp_buffer.h,v 1.1 2006-06-03 10:04:28 phintuka Exp $
 *
 */

#ifndef __UDP_BUFFER_H
#define __UDP_BUFFER_H

#include <stdint.h>

#include "../xine_input_vdr_net.h" // frame headers


#define UDP_BUFFER_SIZE 0x100   // 2^n
#define UDP_BUFFER_MASK 0xff    // 2^n - 1

#if UDP_BUFFER_MASK != UDP_SEQ_MASK
#  error Buffer handling error !!! 
#endif


class cUdpBackLog 
{
    friend class cUdpScheduler;

  private:

    cUdpBackLog(cUdpBackLog&);

    stream_udp_header_t *m_UdpBuffer[UDP_BUFFER_SIZE];
    int m_UdpBufLen[UDP_BUFFER_SIZE];   /* size of allocated memory, not frame */
    int m_PayloadSize[UDP_BUFFER_SIZE]; /* size of frame */
    unsigned int m_SeqNo; /* next (outgoing) sequence number */

  protected:

    cUdpBackLog()
    {
      memset(m_UdpBuffer, 0, sizeof(stream_udp_header_t *)*UDP_BUFFER_SIZE);
      memset(m_UdpBufLen, 0, sizeof(int) * UDP_BUFFER_SIZE);
      memset(m_PayloadSize, 0, sizeof(int) * UDP_BUFFER_SIZE); 
      m_SeqNo = 0;
    }

    void Clear(int HowManyFrames)
    {
      // Clear n last frames from buffer.
      // (called to adjust sequence numbering when some
      // already allocated frames won't be sent)
      //
      // Note: Nothing is freed.
      //       To completely reset buffer it must be deleted and re-created.
      //
      m_SeqNo = (m_SeqNo + UDP_BUFFER_SIZE - HowManyFrames) & UDP_BUFFER_MASK;
    }

    virtual ~cUdpBackLog()
    {
      for(int i=0; i<UDP_BUFFER_SIZE; i++)
	if(m_UdpBuffer[i]) {
	  //m_UdpBufLen[i] = 0;
	  DELETENULL(m_UdpBuffer[i]);
	}
    }

    stream_udp_header_t *Get(int UdpSeqNo)
    {
      int BufIndex = UdpSeqNo & UDP_BUFFER_MASK;
      return m_UdpBuffer[BufIndex];
    }

    int PayloadSize(int UdpSeqNo)
    {
      int BufIndex = UdpSeqNo & UDP_BUFFER_MASK;
      return m_UdpBuffer[BufIndex] ? m_PayloadSize[BufIndex] : 0;
    }

    stream_udp_header_t *MakeFrame(uint64_t StreamPos, 
				   const uchar *Data, int DataLen)
    {
      int UdpPacketLen = DataLen + sizeof(stream_udp_header_t);
      int BufIndex = m_SeqNo & UDP_BUFFER_MASK;
      
      // old buffer too small ? free it
      if(m_UdpBuffer[BufIndex] && m_UdpBufLen[BufIndex] < UdpPacketLen) 
	DELETENULL(m_UdpBuffer[BufIndex]);

      // no buffer ? alloc it
      if(!m_UdpBuffer[BufIndex]) {
	m_UdpBuffer[BufIndex] = (stream_udp_header_t*)new uchar[UdpPacketLen];
	m_UdpBufLen[BufIndex] = UdpPacketLen;
      }
      m_PayloadSize[BufIndex] = DataLen;

      // Fill frame to buffer
      stream_udp_header_t *header = m_UdpBuffer[BufIndex];
      uchar *Payload = UDP_PAYLOAD(header);

      memcpy(Payload, Data, DataLen);
      header->pos = htonll(StreamPos);
      header->seq = htons(m_SeqNo);

      m_SeqNo = (m_SeqNo + 1) & UDP_SEQ_MASK;

      return header;
    }
};


#endif
