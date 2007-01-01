/*
 * vdrdiscovery.h
 *
 * Simple broadcast protocol to search VDR with xineliboutput server 
 * from (local) network.
 *
 * See the main source file 'xineliboutput.c' for copyright information and
 * how to reach the author.
 *
 * $Id: vdrdiscovery.h,v 1.1 2007-01-01 05:04:52 phintuka Exp $
 *
 */

#ifndef _VDRDISCOVERY_H_
#define _VDRDISCOVERY_H_

#define DISCOVERY_MSG_MAXSIZE  1024

#ifdef VDRVERSION
int udp_discovery_init(/*int port*/);
int udp_discovery_broadcast(int fd_discovery, int server_port);
int udp_discovery_recv(int fd_discovery, char *buf, int timeout,
		       struct sockaddr_in *source);
int udp_discovery_is_valid_search(const char *buf);
#else
int udp_discovery_find_server(int *port, char *address);
#endif


#endif // _VDRDISCOVERY_H_
