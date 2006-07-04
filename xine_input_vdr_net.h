/*
 * xine_input_vdr_net.h:  
 *
 * See the main source file 'xineliboutput.c' for copyright information and
 * how to reach the author.
 *
 * $Id: xine_input_vdr_net.h,v 1.2 2006-07-04 01:53:23 phintuka Exp $
 *
 */

#ifndef __XINE_INPUT_VDR_NET_H_
#define __XINE_INPUT_VDR_NET_H_

#include <arpa/inet.h>
#include <endian.h>

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

#if __BYTE_ORDER == __BIG_ENDIAN
#  define ntohll(val)  (val)
#  define htonll(val)  (val)
#  define ntohull(val) (val)
#  define htonull(val) (val)
#else
#  define ntohll(val) ((int64_t)ntohull((uint64_t)val))
#  define htonll(val) ((int64_t)htonull((uint64_t)val))
#  define ntohull(val) \
          ((uint64_t) ntohl((uint32_t)((val) >> 32)) |  \
           (uint64_t) ntohl((uint32_t)(val)) << 32)
#  define htonull(val) \
          ((uint64_t) htonl((uint32_t)((val) >> 32)) |  \
           (uint64_t) htonl((uint32_t)(val)) << 32)
#endif

/*
 * Network packet headers
 */

#if defined __cplusplus
extern "C" {
#endif
  
/*
 * TCP / PIPE 
 */

typedef struct stream_tcp_header {
  uint64_t pos;  /* stream position of first byte */
  uint32_t len;  /* length of following PES packet */
} __attribute__ ((packed)) stream_tcp_header_t;

#define TCP_PAYLOAD(frame) (((uchar *)frame) + sizeof(stream_tcp_header_t))

/*
 * UDP 
 */

typedef struct stream_udp_header {
  uint64_t pos; /* stream position of first byte */
                /* -1ULL and first bytes of frame != 00 00 01 */
                /* --> embedded control stream data */
  uint16_t seq; /* packet sequence number 
		   (for re-ordering and detecting missing packets) */
} __attribute__ ((packed)) stream_udp_header_t;

#define UDP_PAYLOAD(frame) (((uchar *)frame) + sizeof(stream_udp_header_t))
#define UDP_SEQ_MASK 0xff


/* 
 * RTP (RFC 1889): A Transport Protocol for Real-Time Applications
 */

/* RTP data header */
typedef struct stream_rtp_header {
  union {
    uint8_t raw[12];
    struct {
#if __BYTE_ORDER == __BIG_ENDIAN
      unsigned int version:2;   /* protocol version */
      unsigned int padding:1;   /* padding flag */
      unsigned int ext:1;       /* header extension flag */
      unsigned int cc:4;        /* CSRC count */

      unsigned int marker:1;    /* marker bit */
      unsigned int paytype:7;   /* payload type */
#else
      unsigned int cc:4;        /* CSRC count */
      unsigned int ext:1;       /* header extension flag */
      unsigned int padding:1;   /* padding flag */
      unsigned int version:2;   /* protocol version */

      unsigned int paytype:7;   /* payload type */
      unsigned int marker:1;    /* marker bit */
#endif
      uint16_t seq;              /* sequence number */
      uint32_t ts;               /* timestamp */
      uint32_t ssrc;             /* synchronization source */

      /*uint32_t csrc[0];*/      /* optional CSRC list */
    };
  };

  uint8_t payload[0];  

} __attribute__ ((packed)) stream_rtp_header_t;

#define RTP_VERSION         2
#define RTP_MARKER_BIT      0x80
#define RTP_PAYLOAD_TYPE    96     /* application */

#define RTP_VERSION_BYTE    (RTP_VERSION<<6)
#define RTP_PAYLOAD_TYPE_M  (RTP_PAYLOAD_TYPE|RTP_MARKER_BIT)
 
#define RTP_PAYLOAD(frame) (((uchar *)frame) + sizeof(stream_rtp_header_t))

/* RTCP packet types */
typedef enum {
  RTCP_SR   = 200,
  RTCP_RR   = 201,
  RTCP_SDES = 202,
  RTCP_BYE  = 203,
  RTCP_APP  = 204
} rtcp_type_t;

/* RTCP SDES types */
typedef enum {
  RTCP_SDES_END   = 0,
  RTCP_SDES_CNAME = 1,

  RTCP_SDES_NAME  = 2,
  RTCP_SDES_EMAIL = 3,
  RTCP_SDES_PHONE = 4,
  RTCP_SDES_LOC   = 5,
  RTCP_SDES_TOOL  = 6,
  RTCP_SDES_NOTE  = 7,
  RTCP_SDES_PRIV  = 8
} rtcp_sdes_type_t;

/* RTCP common header word */
typedef struct {
  union {
    struct {
#if __BYTE_ORDER == __BIG_ENDIAN
      unsigned int version:2;   /* protocol version */
      unsigned int padding:1;   /* padding flag */
      unsigned int count:5;     /* varies by packet type */
#else
      unsigned int count:5;     /* varies by packet type */
      unsigned int padding:1;   /* padding flag */
      unsigned int version:2;   /* protocol version */
#endif
      unsigned int ptype:8;     /* RTCP packet type */
      
      uint16_t length;          /* pkt len in words, w/o this word */
    };
    uint8_t raw[4];
  };
} rtcp_common_t;

/* RTCP RR (Reception report) */
typedef struct {
  uint32_t ssrc;            /* data source being reported */
  unsigned int fraction:8;  /* fraction lost since last SR/RR */
  int lost:24;              /* cumul. no. pkts lost (signed!) */
  uint32_t last_seq;        /* extended last seq. no. received */
  uint32_t jitter;          /* interarrival jitter */
  uint32_t lsr;             /* last SR packet from this source */
  uint32_t dlsr;            /* delay since last SR packet */
} rtcp_rr_t;
  
/* RTCP SR (Sender report) */
typedef struct {
  uint32_t ssrc;
  uint32_t ntp_sec;  /* NTP timestamp, most significant word / seconds */
  uint32_t ntp_frac;
  uint32_t rtp_ts;
  uint32_t psent;    /* packets sent */
  uint32_t osent;    /* octets sent */
  rtcp_rr_t rr[0];   /* variable-length list */
} rtcp_sr_t;

/* RTCP SDES item */
typedef struct {
  uint8_t type;             /* type of item (rtcp_sdes_type_t) */
  uint8_t length;           /* length of item (in octets) */
  char    data[0];          /* text, not null-terminated */
} rtcp_sdes_item_t;

/* RTCP packet */
typedef struct {
  rtcp_common_t hdr;
  union {
    rtcp_sr_t sr;
    struct {
      uint32_t ssrc;
      rtcp_rr_t rr[0];
    } rr;
    struct {
      uint32_t ssrc;      /* first SSRC/CSRC */
      rtcp_sdes_item_t item[0]; /* list of SDES items */
    } sdes;
    struct {
      uint32_t src[0];   /* list of sources */
      /* can't express trailing text for reason */
    } bye;
  };
} rtcp_packet_t;


#if defined __cplusplus
}
#endif


#endif /*__XINE_INPUT_VDR_NET_H_*/

