/*
 * xine_frontend_vdrdiscovery.c
 *
 * Try to found VDR with xineliboutput server from (local) network.
 *
 * See the main source file 'xineliboutput.c' for copyright information and
 * how to reach the author.
 *
 * $Id: xine_frontend_vdrdiscovery.c,v 1.3 2006-12-14 12:33:44 phintuka Exp $
 *
 */

#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "xine_input_vdr_net.h"  /* default ports */

static int search_vdr_server(int *port, char *address)
{
  struct sockaddr_in sin;
  int fd_broadcast = -1;
  int dummy = 1, trycount = 0;
  struct pollfd pfd;
  char pktbuf[1024];

  *port = DEFAULT_VDR_PORT;
  strcpy(address, "vdr");

  if ((fd_broadcast = socket(PF_INET, SOCK_DGRAM, 0/*IPPROTO_TCP*/)) < 0) {
    LOGERR("fd_broadcast = socket() failed");
  } else {

    if(setsockopt(fd_broadcast, SOL_SOCKET, SO_BROADCAST, 
		  &dummy, sizeof(int)) < 0)
      LOGERR("setsockopt(SO_BROADCAST) failed");
    if(setsockopt(fd_broadcast, SOL_SOCKET, SO_REUSEADDR, 
		  &dummy, sizeof(int)) < 0)
      LOGERR("setsockopt(SO_REUSEADDR) failed");

    sin.sin_family = AF_INET;
    sin.sin_port   = htons(DISCOVERY_PORT);
    sin.sin_addr.s_addr = INADDR_ANY;

    if (bind(fd_broadcast, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
      LOGERR("Can't bind fd_broadcast to %d", DISCOVERY_PORT);
    } else {
      char msg[128];
      sprintf(msg, 
	      DISCOVERY_1_0_HDR  /* "VDR xineliboutput DISCOVERY 1.0" "\r\n" */
 	      DISCOVERY_1_0_CLI  /* "Client: %s:%d" "\r\n" */
	      "\r\n",
	      "255.255.255.255",
	      DISCOVERY_PORT);
      int msglen = strlen(msg);
retry:
      sin.sin_addr.s_addr = INADDR_BROADCAST;
      if(msglen != sendto(fd_broadcast, msg, msglen, 0,
			  (struct sockaddr *)&sin, sizeof(sin))) {
	LOGERR("UDP broadcast send failed (discovery)");
      } else {

	while(1) {
	  int err;
	  pfd.fd = fd_broadcast;
	  pfd.events = POLLIN;

	  errno=0;
	  while(1==(err=poll(&pfd, 1, 500))) {
	    struct sockaddr_in from;
	    socklen_t fromlen = sizeof(from);
	    memset(&from, 0, sizeof(from));
	    memset(pktbuf, 0, sizeof(pktbuf));

	    errno=0;
	    if((err=recvfrom(fd_broadcast, pktbuf, 1023, 0, 
			     (struct sockaddr *)&from, &fromlen)) > 0) {
	      char *mystring = DISCOVERY_1_0_HDR  /* "VDR xineliboutput DISCOVERY 1.0" "\r\n" */
		               "Server port: ";
	      uint32_t tmp = ntohl(from.sin_addr.s_addr);

	      pktbuf[err] = 0;
	      LOGDBG("Reveived broadcast: %d bytes from %d.%d.%d.%d\n   %s", 
		     err, 
		     ((tmp>>24)&0xff), ((tmp>>16)&0xff), 
		     ((tmp>>8)&0xff), ((tmp)&0xff),
		     pktbuf);
	      if(!strncmp(mystring, pktbuf, strlen(mystring))) {
		LOGDBG("Valid discovery message");
		close(fd_broadcast);
		sprintf(address, "%d.%d.%d.%d",
			((tmp>>24)&0xff), ((tmp>>16)&0xff), 
			((tmp>>8)&0xff), ((tmp)&0xff));
		if(1==sscanf(pktbuf+strlen(mystring), "%d", port))
		  return 1;
	      } else {
		LOGDBG("NOT valid discovery message");
	      }
	    } else {
	      LOGERR("broadcast recvfrom failed");
	      break;
	    }
	  } 
	  if(!err) {
	    /* timeout */
	    trycount++;
	    if(trycount < 3)
	      goto retry;
	    break;
	  }
	  LOGERR("broadcast poll error");
	  break;
	}
      }
    }    
  }

  /* failed */
  close(fd_broadcast);
  return 0;
}

