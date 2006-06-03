/*
 * xine_input_vdr_net.h:  
 *
 * See the main source file 'xineliboutput.c' for copyright information and
 * how to reach the author.
 *
 * $Id: xine_input_vdr_net.h,v 1.1 2006-06-03 09:50:54 phintuka Exp $
 *
 */

#ifndef __XINE_INPUT_VDR_NET_H_
#define __XINE_INPUT_VDR_NET_H_

#include <arpa/inet.h>


/*
 * Default port(s)
 */

#ifndef DEFAULT_VDR_PORT
#  define DEFAULT_VDR_PORT 37890
#endif
#ifndef DISCOVERY_PORT
#  define DISCOVERY_PORT 37890
#endif

/*
 * Byte-order conversions
 */

#define ntohll(val) ((int64_t)ntohull((uint64_t)val))
#define htonll(val) ((int64_t)htonull((uint64_t)val))
  
#define ntohull(val) \
  (ntohs(0x1234) == 0x1234 ? (val) : \
          (uint64_t) ntohl((uint32_t)((val) >> 32)) |  \
          (uint64_t) ntohl((uint32_t)(val)) << 32)

#define htonull(val) \
  (ntohs(0x1234) == 0x1234 ? (val) : \
          (uint64_t) htonl((uint32_t)((val) >> 32)) |  \
          (uint64_t) htonl((uint32_t)(val)) << 32)


/*
 * Network packet headers
 */

#if defined __cplusplus
extern "C" {
#endif


#if 0
typedef struct stream_rtp_header {
  /* input_rtp.c ? */
  int dummy;
} __attribute__ ((packed)) stream_rtp_header_t;

#define RTP_PAYLOAD(frame) (((uchar *)frame) + sizeof(stream_rtp_header_t))
#endif


#define UDP_SEQ_MASK 0xff

typedef struct stream_udp_header {
  uint64_t pos; /* stream position of first byte */
                /* -1ULL and first bytes of frame != 00 00 01 */
                /* --> embedded control stream data */
  uint16_t seq; /* packet sequence number 
		   (for re-ordering and detecting missing packets) */
} __attribute__ ((packed)) stream_udp_header_t;

#define UDP_PAYLOAD(frame) (((uchar *)frame) + sizeof(stream_udp_header_t))


typedef struct stream_tcp_header {
  uint64_t pos; /* stream position of first byte */
  uint32_t len; /* length of following PES packet */
} __attribute__ ((packed)) stream_tcp_header_t;

#define TCP_PAYLOAD(frame) (((uchar *)frame) + sizeof(stream_tcp_header_t))

#if defined __cplusplus
}
#endif


#endif /*__XINE_INPUT_VDR_NET_H_*/

