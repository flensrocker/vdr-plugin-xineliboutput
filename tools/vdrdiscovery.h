/*
 * vdrdiscovery.h
 *
 * Simple broadcast protocol to search VDR with xineliboutput server 
 * from (local) network.
 *
 * See the main source file 'xineliboutput.c' for copyright information and
 * how to reach the author.
 *
 * $Id: vdrdiscovery.h,v 1.2 2007-01-01 06:17:11 phintuka Exp $
 *
 */

#ifndef _VDRDISCOVERY_H_
#define _VDRDISCOVERY_H_

#define DISCOVERY_MSG_MAXSIZE  1024

#ifdef __cplusplus
extern "C" {
#endif

#ifdef FE_STANDALONE
int udp_discovery_find_server(int *port, char *address);
#else
int udp_discovery_init(void);
int udp_discovery_broadcast(int fd_discovery, int server_port);
int udp_discovery_recv(int fd_discovery, char *buf, int timeout,
		       struct sockaddr_in *source);
int udp_discovery_is_valid_search(const char *buf);
#endif

#ifdef __cplusplus
};
#endif


#endif // _VDRDISCOVERY_H_
