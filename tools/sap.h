/*
 * sap.h: RFC2974 Session Announcement Protocol (SAP) version 2
 *
 * See the main source file 'xineliboutput.c' for copyright information and
 * how to reach the author.
 *
 * $Id: sap.h,v 1.1 2006-12-07 22:15:05 uid218003 Exp $
 *
 */

#ifndef XINELIBOUTPUT_SAP_H_
#define XINELIBOUTPUT_SAP_H_

#include <arpa/inet.h>
#include <endian.h>


#define SAP_IP_ADDRESS "224.2.127.254" /* SAPv1 IP4 global scope multicast address */
#define SAP_IP_TTL     255
#define SAP_UDP_PORT   9875


typedef struct {

  /* RFC2974: SAP (Session Announcement Protocol) version 2 PDU */

  union {
    struct {
#if __BYTE_ORDER == __BIG_ENDIAN
      uint8_t version    : 3;
      uint8_t addr_type  : 1;
      uint8_t reserved   : 1;
      uint8_t msg_type   : 1;
      uint8_t encrypted  : 1;
      uint8_t compressed : 1;
#else
      uint8_t compressed : 1;
      uint8_t encrypted  : 1;
      uint8_t msg_type   : 1;
      uint8_t reserved   : 1;
      uint8_t addr_type  : 1;
      uint8_t version    : 3;
#endif
    };
    uint8_t raw0;
  };
  
  uint8_t  auth_len;
  uint16_t msgid_hash;

  union {
    uint8_t  u8[4];
    uint32_t u32;
  } ip4_source;
  
  char payload[0];

}  __attribute__((packed)) sap_pdu_t;


static inline sap_pdu_t *sap_create_pdu(uint32_t src_ip, 
					uint16_t msgid,
					int announce, 
					const char *payload_type, 
					const char *payload)
{
  sap_pdu_t *pdu;
  int length = sizeof(sap_pdu_t) + strlen(payload) + 3;

  if(payload_type)
    length += strlen(payload_type);

  pdu = (sap_pdu_t*)malloc(length);

  memset(pdu, 0, sizeof(sap_pdu_t));
  pdu->version    = 1;  /* SAP v1 / v2 */
  pdu->msg_type   = announce ? 0 : 1;
  pdu->msgid_hash = msgid;
  pdu->ip4_source.u32 = src_ip;

  if(payload_type) {
    char *tmp = &pdu->payload[0];
    strcpy(tmp, payload_type);
    tmp += strlen(tmp) + 1;
    strcpy(tmp, payload);
  } else {
    /* payload type defaults to application/sdp */
    sprintf(&pdu->payload[0], "%s%c%c", payload, 0, 0);
  }

  return pdu;
}

static inline int sap_compress_pdu(sap_pdu_t *pdu)
{
  /* zlib compression */

  /* not implemented */

  pdu->compressed = 0;
  return -1;
}

static inline int sap_send_pdu(sap_pdu_t *pdu, uint32_t dst_ip)
{
  int len = 0, r;
  int iReuse = 1, iLoop = 1, iTtl = SAP_IP_TTL;
  int fd = socket(AF_INET, SOCK_DGRAM, 0);

  if(fd < 0) {
    LOGERR("socket() failed (UDP/SAP multicast)");
    return -1;
  }

  setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &iReuse, sizeof(int));
  setsockopt(fd, IPPROTO_IP, IP_MULTICAST_TTL, &iTtl, sizeof(int));
  setsockopt(fd, IPPROTO_IP, IP_MULTICAST_LOOP, &iLoop, sizeof(int));

  // Connect to multicast address
  struct sockaddr_in sin;
  sin.sin_family = AF_INET;
  sin.sin_port = htons(SAP_UDP_PORT);
  sin.sin_addr.s_addr = dst_ip ? dst_ip : inet_addr(SAP_IP_ADDRESS);
  
  if(connect(fd, (struct sockaddr *)&sin, sizeof(sin))==-1) 
    LOGERR("UDP/SAP multicast connect() failed.");
    
  // Set to non-blocking mode
  fcntl (fd, F_SETFL, fcntl (fd, F_GETFL) | O_NONBLOCK);

  // size of PDU
  len += strlen(&pdu->payload[0]);
  if(!strstr(&pdu->payload[0], "\r\n")) {
    /* assume mime content type is present */
    len += 1;
    len += strlen(&pdu->payload[len]);
    len += sizeof(sap_pdu_t);
  }

  // network order
  pdu->msgid_hash = htons(pdu->msgid_hash);

  // send
  r = send(fd, pdu, len, 0);
  if(r < 0)
    LOGERR("UDP/SAP multicast send() failed.");

#if 0
  /* log PDU */
  for(int i=0; i<len;) {
    char x[4096]="", a[4096]="";
    for(int j=0; j<16 && i<len; i++, j++) {
      char t[8], ch = ((char*)pdu)[i];
      sprintf(t, "%02X ", ((unsigned int)ch)&0xff);
      strcat(x, t);
      sprintf(t, "%c", (ch>=32 && ch<127) ? ch : '.');
      strcat(a, t);
    }
    LOGMSG("SAP: 0x%02x: %-50s%-18s",  i/16-1, x, a);
  }
#endif

  // back to host order
  pdu->msgid_hash = ntohs(pdu->msgid_hash);

  return r == len ? len : -1;
}

#endif /* XINELIBOUTPUT_SAP_H_ */
