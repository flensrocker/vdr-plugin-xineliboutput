/*
 * frontend_svr.c: server for remote frontends
 *
 * See the main source file 'xineliboutput.c' for copyright information and
 * how to reach the author.
 *
 * $Id: frontend_svr.c,v 1.20 2006-09-03 12:11:50 phintuka Exp $
 *
 */

#define __STDC_FORMAT_MACROS
#include <inttypes.h>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <time.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <netinet/tcp.h>

#include <vdr/config.h>
#include <vdr/tools.h>
#include <vdr/plugin.h>

#include "logdefs.h"
#include "config.h"

#include "xine_input_vdr_net.h"   // stream header(s)
#include "xine_osd_command.h"     // osd commands

#include "tools/cxsocket.h"
#include "tools/future.h"
#include "tools/backgroundwriter.h"
#include "tools/udp_pes_scheduler.h"

#include "frontend_svr.h"
#include "device.h"

#define LOG_OSD_BANDWIDTH (128*1024)  /* log messages if OSD bandwidth > 1 Mbit/s */

typedef struct {
  int    Size;
  uchar *Data;
} grab_result_t;

class cStcFuture : public cFuture<int64_t> {};
class cReplyFuture : public cFuture<int>, public cListObject {};
class cGrabReplyFuture : public cFuture<grab_result_t>, public cListObject {};
class cCmdFutures : public cHash<cReplyFuture> {};

#define POLLING_INTERVAL (10*1000)

//----------------------------- cXinelibServer --------------------------------

cXinelibServer::cXinelibServer(int listen_port) :
  cXinelibThread("Remote decoder/display server (cXinelibServer)") 
{
  int i;
  for(i=0; i<MAXCLIENTS; i++) {
    fd_data[i] = -1;
    fd_control[i] = -1;
    m_Writer[i] = NULL;
    m_bMulticast[i] = 0;
    m_bConfigOk[i] = false;
    m_bUdp[i] = 0;
  }

  m_Port = listen_port;

  fd_listen    = -1;
  fd_multicast = -1;
  fd_rtcp = -1;
  fd_discovery = -1;

  m_iMulticastMask = 0;
  m_iUdpFlowMask   = 0;

  m_Master     = false;

  m_Scheduler  = new cUdpScheduler;
  m_StcFuture  = new cStcFuture;
  m_Futures    = new cCmdFutures;

  cString Base(cPlugin::ConfigDirectory());
  if(*Base)
    m_PipesDir = cString::sprintf("%s/xineliboutput/pipes", *Base);
  else
    m_PipesDir = cString("/tmp/xineliboutput/pipes");

  m_Token = 1;
}

cXinelibServer::~cXinelibServer() 
{
  int i;
  
  CLOSESOCKET(fd_listen);
  CLOSESOCKET(fd_discovery);
  CLOSESOCKET(fd_multicast);
  CLOSESOCKET(fd_rtcp);
  
  for(i=0; i<MAXCLIENTS; i++) 
    CloseConnection(i);
  
  delete m_StcFuture;
  delete m_Futures;
  delete m_Scheduler;
}

void cXinelibServer::Stop(void)
{
  int i;

  TRACEF("cXinelibServer::Stop");

  SetStopSignal();

  CLOSESOCKET(fd_listen);
  CLOSESOCKET(fd_discovery);
  CLOSESOCKET(fd_multicast);
  CLOSESOCKET(fd_rtcp);

  for(i=0; i<MAXCLIENTS; i++) 
    CloseConnection(i);

  cXinelibThread::Stop();
}

void cXinelibServer::Clear(void)
{
  TRACEF("cXinelibServer::Clear");

  LOCK_THREAD;

  cXinelibThread::Clear();

  for(int i=0; i<MAXCLIENTS; i++) 
    if(fd_control[i] >= 0 && fd_data >= 0 && m_Writer[i]) 
      m_Writer[i]->Clear();

  if(m_Scheduler)
    m_Scheduler->Clear();
}

#define CloseDataConnection(cli)       \
  do {                                 \
    if(m_bUdp[cli] && fd_data[cli]>=0) \
      m_Scheduler->RemoveHandle(fd_data[cli]); \
    CLOSESOCKET(fd_data[cli]);         \
    if(m_Writer[cli]) {                \
      delete m_Writer[cli];            \
      m_Writer[cli] = NULL;            \
    }                                  \
    m_iUdpFlowMask &= ~(1<<cli);       \
    m_iMulticastMask &= ~(1<<cli);     \
    if(!m_iMulticastMask && !xc.remote_rtp_always_on) \
      m_Scheduler->RemoveHandle(fd_multicast); \
    m_bUdp[cli] = false;               \
    m_bMulticast[cli] = false;         \
    m_bConfigOk[cli] = false;          \
  } while(0)

void cXinelibServer::CloseConnection(int cli)
{
  CloseDataConnection(cli);
  if(fd_control[cli]>=0) {
    LOGMSG("Closing connection %d", cli);
    CLOSESOCKET(fd_control[cli]);
    cXinelibDevice::Instance().ForcePrimaryDevice(false);
  }
}

static int recompress_osd_net(uint8_t *raw, xine_rle_elem_t *data, int elems)
{
  uint8_t *raw0 = raw;
  for(int i=0; i<elems; i++) {
    uint16_t len = data[i].len;
    uint16_t color = data[i].color;
    if(len >= 0x80) {
      *(raw++) = (len>>8) | 0x80;
      *(raw++) = (len & 0xff);
    } else {
      *(raw++) = (len & 0x7f);
    }
    *(raw++) = color;
  }
  return (raw-raw0);
}

void cXinelibServer::OsdCmd(void *cmd_gen)
{
  TRACEF("cXinelibServer::OsdCmd");
  int i;
  
  LOCK_THREAD;

  // check if there are any clients 
  if(!HasClients())
    return;

  if(cmd_gen) {
    osd_command_t *cmd = (osd_command_t*)cmd_gen;
    osd_command_t cmdnet;
#if __BYTE_ORDER == __LITTLE_ENDIAN
    /* -> network order */
    memset(&cmdnet, 0, sizeof(osd_command_t));
    cmdnet.cmd = htonl(cmd->cmd);
    cmdnet.wnd = htonl(cmd->wnd);
    cmdnet.pts = htonll(cmd->pts);
    cmdnet.delay_ms = htonl(cmd->delay_ms);
    cmdnet.x = htons(cmd->x);
    cmdnet.y = htons(cmd->y);
    cmdnet.w = htons(cmd->w);
    cmdnet.h = htons(cmd->h);
    cmdnet.datalen = htonl(cmd->datalen);
    cmdnet.num_rle = htonl(cmd->num_rle);
    cmdnet.colors  = htonl(cmd->colors);
    if(cmd->data) {
      cmdnet.raw_data = (uint8_t*)malloc(cmd->datalen);
      cmdnet.datalen = htonl( recompress_osd_net(cmdnet.raw_data, cmd->data, cmd->num_rle));
    } else {
      cmdnet.data = NULL;
    }
    cmdnet.palette = cmd->palette;
#elif __BYTE_ORDER == __BIG_ENDIAN
    memcpy(&cmdnet, cmd, sizeof(osd_command_t));
    cmdnet.raw_data = (uint8_t *)malloc(cmd->datalen);
    cmdnet.datalen = recompress_osd_net(cmdnet.raw_data, cmd->data, cmd->num_rle);
#else
#  error __BYTE_ORDER not defined !
#endif
    
    for(i=0; i<MAXCLIENTS; i++)
      if(fd_control[i] >= 0 && 
	 !write_osd_command(fd_control[i], &cmdnet)) {
	LOGMSG("Send OSD command failed");
        CloseConnection(i);
      }

    free(cmdnet.data);

#ifdef LOG_OSD_BANDWIDTH
    {
      static int64_t timer = 0LL;
      static int bytes = 0;
      int64_t now = cTimeMs::Now();
      
      if(timer + 5000LL < now) {
	timer = now;
	bytes = 0;
      } else if(timer + 1000LL < now) {
	bytes = bytes / (((int)(now - timer)) / 1000);
	if(bytes > LOG_OSD_BANDWIDTH)
	  LOGMSG("OSD bandwidth: %d bytes/s (%d kbit/s)", bytes, bytes*8/1024);
	timer = now;
	bytes = 0;
      }
      bytes += sizeof(osd_command_t) + ntohl(cmdnet.datalen);
    }
#endif
  }
}


int64_t cXinelibServer::GetSTC(void)
{
  Lock();

  // check if there are any clients 
  if(!HasClients()) {
    Unlock();
    return -1ULL;
  }

  // Query client(s)
  m_StcFuture->Reset();
  Xine_Control("GETSTC");

  Unlock();

  if(! m_StcFuture->Wait(200)) {
    LOGMSG("cXinelibServer::GetSTC timeout (200ms)");
    return -1ULL;
  }
    
  //if(delay.Elapsed() > 0 && !is_Paused)
  //  LOGMSG("GetSTC: compensating network delay by %s ticks (ms)\n",
  //         delay.Elapsed()*90000/2, delay.Elapsed()/2);

  return m_StcFuture->Value() /*+ (delay.Elapsed()*90000/2*/;
}

int cXinelibServer::Play_PES(const uchar *data, int len)
{
  int TcpClients = 0, UdpClients = 0, RtpClients = 0;

  if(!m_bLiveMode && m_Master) {
    // dvbplayer feeds multiple pes packets after each poll 
    // (all frames between two pictures).
    // So, we must poll here again to avoid overflows ...
    static cPoller dummy;
    if(!Poll(dummy, 3))
      return 0;
  }

  LOCK_THREAD; // Lock control thread out

  for(int i=0; i<MAXCLIENTS; i++) {
    if(fd_control[i] >= 0 && m_bConfigOk[i]) {
      if(fd_data[i] >= 0) {
	if(m_bUdp[i]) {
# if 0
	  if(m_iUdpFlowMask & (1<<i)) {
	    LOGDBG("UDP full signal in Play_PES, sleeping 5ms");
	    cCondWait::SleepMs(5);
	  }
# endif
	  UdpClients++;
	  
	} else if(m_Writer[i]) {
	  int result = m_Writer[i]->Put(m_StreamPos, data, len);
	  if(!result) {
	    LOGMSG("cXinelibServer::Play_PES Write/Queue error (TCP/PIPE)");
	    CloseConnection(i);
	  } else if(result<0) {
	    LOGMSG("cXinelibServer::Play_PES Buffer overflow (TCP/PIPE)");
	  }
	  
	  TcpClients++;	
	}
      }
    }
  }

  RtpClients = ((m_iMulticastMask || xc.remote_rtp_always_on) && fd_multicast >= 0);

  if(UdpClients || RtpClients)
    if(! m_Scheduler->Queue(m_StreamPos, data, len))
      LOGMSG("cXinelibServer::Play_PES Buffer overflow (UDP/RTP)");
  
  if(TcpClients || UdpClients || RtpClients) 
    cXinelibThread::Play_PES(data, len);

  return len;
}


void cXinelibServer::Xine_Sync(void)
{
#if 0
  TRACEF("cXinelibServer::Xine_Sync");

  bool foundReceivers=false;
  SyncLock.Lock();
  cXinelibThread::Xine_Control((const char *)"SYNC",m_StreamPos);
  for(int i=0; i<MAXCLIENTS; i++)
    if(fd_control[i] >= 0) {
      wait_sync[i]=true;
      foundReceivers=true;
    }
  Unlock();
  if(foundReceivers) {
    TRACE("cXinelibServer::Xine_Sync --> WAIT...");
    SyncDone.Wait(SyncLock); 
    TRACE("cXinelibServer::Xine_Sync --> WAIT DONE.");
  }
  SyncLock.Unlock();
  Lock();
#endif
}


bool cXinelibServer::Poll(cPoller &Poller, int TimeoutMs) 
{
  // in live mode transponder clock is the master ... 
  if((*xc.local_frontend && strncmp(xc.local_frontend, "none", 4)) || m_bLiveMode)
    return m_Scheduler->Poll(TimeoutMs, m_Master=false);

  // replay mode:
  do {
    Lock();
    m_Master = true;
    int Free = 0xffff, Clients = 0;
    for(int i=0; i<MAXCLIENTS; i++) {
      if(fd_control[i]>=0 && m_bConfigOk[i]) 
	if(fd_data[i]>=0 || m_bMulticast[i]) {
	  if(m_Writer[i])
	    Free = min(Free, m_Writer[i]->Free());
	  Clients++;
	}
    }
    Unlock();
    
    // replay is paused when no clients 
    if(!Clients && !xc.remote_rtp_always_on) {
      if(TimeoutMs>0)
	cCondWait::SleepMs(TimeoutMs);
      return false;
    }

    // in replay mode cUdpScheduler is master timing source 
    if(Free < 8128 || !m_Scheduler->Poll(TimeoutMs, true)) {
      if(TimeoutMs > 0)
	cCondWait::SleepMs(min(TimeoutMs, 5));
      TimeoutMs -= 5;
    } else {
      return true;
    }
    
  } while(TimeoutMs > 0);
  
  return false;
}

bool cXinelibServer::Flush(int TimeoutMs) 
{
  int  result = true;

  if(m_Scheduler)
    result = m_Scheduler->Flush(TimeoutMs) && result;
  
  for(int i=0; i<MAXCLIENTS; i++) 
    if(fd_control[i]>=0 && fd_data[i]>=0 && m_Writer[i])
      result = m_Writer[i]->Flush(TimeoutMs) && result;

  if(TimeoutMs > 50)
    TimeoutMs  = 50;

  if(result) {
    char tmp[64];
    sprintf(tmp, "FLUSH %d %" PRIu64 " %d", TimeoutMs, m_StreamPos, m_Frames);
    result = (PlayFileCtrl(tmp)) <= 0 && result;
  }

  return result;
}

int cXinelibServer::Xine_Control(const char *cmd)
{
  TRACEF("cXinelibServer::Xine_Control");

  if(cmd && *cmd) {
    char buf[2048];
    int len;
    sprintf(buf, "%s\r\n", cmd);
    len = strlen(buf);

    LOCK_THREAD;

    for(int i=0; i<MAXCLIENTS; i++)
      if(fd_control[i]>=0 && (fd_data[i]>=0 || m_bMulticast[i]) && m_bConfigOk[i])
	if(len != timed_write(fd_control[i], buf, len, 100)) {
	  LOGMSG("Control send failed (%s), dropping client", cmd);
	  CloseConnection(i);
	}
  }

  return 1;
}

int cXinelibServer::Xine_Control_Sync(const char *cmd)
{
  TRACEF("cXinelibServer::Xine_Control_Sync");

  if(cmd && *cmd) {
    int UdpClients = 0, RtpClients = 0;
    char buf[2048];
    int len;
    sprintf(buf, "%s\r\n", cmd);
    len = strlen(buf);

    LOCK_THREAD;

    for(int i=0; i<MAXCLIENTS; i++)
      if(fd_control[i]>=0 && m_bConfigOk[i]) {
	if(fd_data[i] >= 0) {
	  if(m_bUdp[i])
	    UdpClients++;
	  else if(m_Writer[i]) 
	    m_Writer[i]->Put((uint64_t)(-1ULL), (const uchar*)buf, len);
	}
      }

    RtpClients = ((m_iMulticastMask || xc.remote_rtp_always_on) && fd_multicast >= 0);

    if(UdpClients || RtpClients)
      if(! m_Scheduler->Queue((uint64_t)(-1ULL), (const uchar*)buf, len)) 
	LOGMSG("cXinelibServer::Xine_Control_Sync overflow (UDP/RTP)");
  }
  
  return 1;
}

void cXinelibServer::TrickSpeed(int Speed)
{
  if(Speed == 0)
    m_Scheduler->Pause(true);
  else
    m_Scheduler->Pause(false);

  cXinelibThread::TrickSpeed(Speed);
}

bool cXinelibServer::EndOfStreamReached(void)
{
  LOCK_THREAD;
 
  /* Check if there are any clients */
  if(!HasClients())
    return true;

  return cXinelibThread::EndOfStreamReached();
}

int cXinelibServer::AllocToken(void)
{
  LOCK_THREAD;

  m_Token = (m_Token+1) & 0xffff;

  cXinelibThread::Xine_Control((const char *)"TOKEN", m_Token);

  return m_Token;
}

bool cXinelibServer::HasClients(void)
{
  LOCK_THREAD;

  int i;
  for(i=0; i<MAXCLIENTS; i++) 
    if(fd_control[i]>=0 && m_bConfigOk[i])  
      return true;

  return false;
}

int cXinelibServer::PlayFileCtrl(const char *Cmd)
{
  /* Check if there are any clients */
  if(!HasClients())
    return -1;

  bool bPlayfile = false /*, bGet = false, bFlush = false*/;
  if((!strncmp(Cmd, "FLUSH", 5)    /*&& (bFlush=true)*/) ||
     (!strncmp(Cmd, "PLAYFILE", 8) && (bPlayfile=true)) ||
     (!strncmp(Cmd, "GET", 3)      /*&& (bGet=true)*/)) {  // GETPOS, GETLENGTH, ...
     
    Lock();  
  
    /* Get token, send it to client and set future for it */
    int token = AllocToken();
    cReplyFuture future;
    m_Futures->Add(&future, token);

    /* Send actual command */
    cXinelibThread::PlayFileCtrl(Cmd);

    Unlock();

    /* When server thread get REPLY %d %d (first %d == token, second returned value)
     * it sets corresponding future (by token; if found) in list
     * and removes it from list.
     */

#ifdef XINELIBOUTPUT_DEBUG
    int64_t t = cTimeMs::Now();
#endif

    int timeout = 300;
    if(bPlayfile)
      timeout = 5000;

    future.Wait(timeout);

    Lock();
    m_Futures->Del(&future, token);
    Unlock();

    if(!future.IsReady()) {
      LOGMSG("cXinelibServer::PlayFileCtrl: Timeout (%s , %d ms) %d", Cmd, timeout, token);
      return -1;
    }

    TRACE("cXinelibServer::PlayFileCtrl("<<Cmd<<"): result=" << future.Value()
	  << " delay: " << (int)(cTimeMs::Now()-t) << "ms"); 

    if(bPlayfile)
      m_bEndOfStreamReached = false;

    return future.Value();
  }
  
  return cXinelibThread::PlayFileCtrl(Cmd);
}


bool cXinelibServer::Listen(int listen_port) 
{
  LOCK_THREAD;

  bool result = false;
  TRACEF("cXinelibServer::Listen");

  if(listen_port <= 0 || listen_port > 0xffff) {
    CLOSESOCKET(fd_listen);
    CLOSESOCKET(fd_discovery);
    if(fd_multicast >= 0 && m_Scheduler)
      m_Scheduler->RemoveHandle(fd_multicast);
    CLOSESOCKET(fd_multicast);
    CLOSESOCKET(fd_rtcp);
    LOGMSG("Not listening for remote connections");
    return false;
  }

  if(fd_listen<0 || listen_port != m_Port) {
    m_Port = listen_port;
    CLOSESOCKET(fd_listen);
    if(m_Port>0) {
      int iReuse = 1;
      struct sockaddr_in name;
      name.sin_family = AF_INET;
      name.sin_addr.s_addr = htonl(INADDR_ANY);
      name.sin_port = htons(m_Port);

      fd_listen = socket(PF_INET,SOCK_STREAM,0);
      setsockopt(fd_listen, SOL_SOCKET, SO_REUSEADDR, &iReuse, sizeof(int));

      if (bind(fd_listen, (struct sockaddr *)&name, sizeof(name)) < 0) {
	LOGERR("cXinelibServer: bind error (port %d): %s", 
	       m_Port, strerror(errno));
        CLOSESOCKET(fd_listen);
      } else if(listen(fd_listen, MAXCLIENTS)) {
	LOGERR("cXinelibServer: listen error (port %d): %s", 
	       m_Port, strerror(errno));
        CLOSESOCKET(fd_listen);
      } else {
	LOGMSG("Listening on port %d", m_Port);
	result = true;
      }
    }
  }

  // set listen for discovery messages
  CLOSESOCKET(fd_discovery);
  if(xc.remote_usebcast) {
    struct sockaddr_in sin;
    if ((fd_discovery = socket(PF_INET, SOCK_DGRAM, 0/*IPPROTO_TCP*/)) < 0) {
      LOGERR("socket() failed (UDP discovery)");
    } else {
      int iBroadcast = 1, iReuse = 1;
      setsockopt(fd_discovery, SOL_SOCKET, SO_BROADCAST, &iBroadcast, sizeof(int));
      setsockopt(fd_discovery, SOL_SOCKET, SO_REUSEADDR, &iReuse, sizeof(int));
      sin.sin_family = AF_INET;
      sin.sin_port   = htons(DISCOVERY_PORT);
      sin.sin_addr.s_addr = INADDR_BROADCAST;  //INADDR_ANY grabs rtp too ...

      if (bind(fd_discovery, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
	LOGERR("bind() failed (UDP discovery)");
	CLOSESOCKET(fd_discovery);
      } else {  
	if(udp_discovery_broadcast(fd_discovery, m_Port) < 0)
	  CLOSESOCKET(fd_discovery);
	else
	  LOGMSG("Listening for UDP broadcasts on port %d", m_Port);
      }
    }
  }

  // set up multicast sockets

  if(fd_multicast >= 0 && m_Scheduler)
    m_Scheduler->RemoveHandle(fd_multicast);
  CLOSESOCKET(fd_multicast);
  CLOSESOCKET(fd_rtcp);

  if(xc.remote_usertp) {
    //
    // RTP
    //
    if((fd_multicast = socket(AF_INET, SOCK_DGRAM, 0)) < 0) 
      LOGERR("socket() failed (UDP/RTP multicast)");

    // Set buffer sizes
    set_socket_buffers(fd_multicast, KILOBYTE(256), 2048);

    // Set multicast socket options
    if(set_multicast_options(fd_multicast, xc.remote_rtp_ttl))
      CLOSESOCKET(fd_multicast);

    // Connect to multicast address
    struct sockaddr_in sin;
    sin.sin_family = AF_INET;
    sin.sin_port = htons(xc.remote_rtp_port);
    sin.sin_addr.s_addr = inet_addr(xc.remote_rtp_addr);

    if(connect(fd_multicast, (struct sockaddr *)&sin, sizeof(sin))==-1 && 
       errno != EINPROGRESS) 
      LOGERR("connect(fd_multicast) failed. Address=%s, port=%d",
	     xc.remote_rtp_addr, xc.remote_rtp_port);
    
    // Set to non-blocking mode
    if(fcntl (fd_multicast, F_SETFL, 
	      fcntl (fd_multicast, F_GETFL) | O_NONBLOCK) == -1) 
      LOGERR("can't put multicast socket in non-blocking mode");      
    
    //
    // RTCP
    //
    if((fd_rtcp = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
      LOGERR("socket() failed (RTCP multicast)");

    /* RTCP port (RFC 1889) */
    if(xc.remote_rtp_port & 1)
      sin.sin_port = htons(xc.remote_rtp_port - 1);
    else
      sin.sin_port = htons(xc.remote_rtp_port + 1);
    
    set_socket_buffers(fd_rtcp, 16384, 16384);
    if(set_multicast_options(fd_rtcp, xc.remote_rtp_ttl))
      CLOSESOCKET(fd_rtcp);
    
    if(connect(fd_rtcp, (struct sockaddr *)&sin, sizeof(sin))==-1 && 
       errno != EINPROGRESS) 
      LOGERR("connect(fd_rtcp) failed. Address=%s, port=%d",
	     xc.remote_rtp_addr, xc.remote_rtp_port +
	     (xc.remote_rtp_port&1)?-1:1);
    
    // Set to non-blocking mode
    if(fcntl (fd_rtcp, F_SETFL, 
	      fcntl (fd_rtcp, F_GETFL) | O_NONBLOCK) == -1) 
      LOGERR("can't put multicast socket in non-blocking mode");
    
    // Finished

    if(fd_multicast >= 0) {
      if(xc.remote_rtp_always_on)
	LOGMSG("WARNING: RTP Configuration: transmission is always on !");
      if(xc.remote_rtp_always_on || m_iMulticastMask)
	m_Scheduler->AddHandle(fd_multicast, fd_rtcp);
    }
  }

  return result;
}

uchar *cXinelibServer::GrabImage(int &Size, bool Jpeg, 
				 int Quality, int SizeX, int SizeY) 
{
  cGrabReplyFuture future;
  char  cmd[64];
  uchar *result = NULL;

  Lock();

  /* Check if there are any clients */
  if(!HasClients())
    return NULL;

  sprintf(cmd, "GRAB %s %d %d %d\r\n", 
	  Jpeg ? "JPEG" : "PNM", Quality, SizeX, SizeY);

  int token = AllocToken();
  m_Futures->Add(&future, token);

  // might be better to request iamge from one client only (?)
  Xine_Control(cmd);

  Unlock();

  if(future.Wait(5000)) {
    grab_result_t r = future.Value(); 
    if((Size = r.Size) > 0) {
      LOGDBG("cXinelibServer::GrabImage: image size is %d bytes", Size);
      result = r.Data;
    } else {
      LOGMSG("cXinelibServer::Grab: Grab failed (%d)", Size);
    }
  } else {
    LOGMSG("cXinelibServer::Grab: Timeout (5000 ms)");
  }

  Lock();
  m_Futures->Del(&future, token);
  Unlock();

  return result;
}

//
// (Client) Control message handling
//

#define CREATE_NEW_WRITER \
  if(m_Writer[cli]) \
    delete m_Writer[cli]; \
  m_Writer[cli] = new cBackgroundWriter(fd); 

void cXinelibServer::Handle_Control_PIPE(int cli, const char *arg)
{
  char buf[256];
  LOGDBG("Trying PIPE connection ...");

  CloseDataConnection(cli);

  //
  // TODO: client should create pipe; waiting here is not good thing ...
  //

  if(!xc.remote_usepipe) {
    LOGMSG("PIPE transport disabled in configuration");
    write_cmd(fd_control[cli], "PIPE NONE\r\n");
    return;
  }

  MakeDirs(*m_PipesDir, true);

  int i;
  char pipeName[1024];
  for(i=0; i<10; i++) {
    sprintf(pipeName,"%s/pipe.%d", *m_PipesDir, i);
    if(mknod(pipeName, 0644|S_IFIFO, 0) < 0) {
      unlink(pipeName);
      continue;
    }
    else
      break;
  }
  if(i>=10) {
    LOGERR("Pipe creation failed (%s)", pipeName);
    RemoveFileOrDir(*m_PipesDir, false);
    write_cmd(fd_control[cli], "PIPE NONE\r\n");
    return;
  }

  sprintf(buf, "PIPE %s\r\n", pipeName);
  write_cmd(fd_control[cli], buf);

  cPoller poller(fd_control[cli],false);
  poller.Poll(500); /* quite short time ... */

  int fd;
  if((fd = open(pipeName, O_WRONLY|O_NONBLOCK)) < 0) {
    LOGDBG("Pipe not opened by client");
    /*write_cmd(fd_control[cli], "PIPE NONE\r\n");*/
    unlink(pipeName); 
    RemoveFileOrDir(*m_PipesDir, false);
    return;
  }

  fcntl(fd, F_SETFL, fcntl(fd, F_GETFL)|O_NONBLOCK);

  LOGDBG("cXinelibServer::Handle_Control: pipe %s open", pipeName);

  unlink(pipeName); /* safe to remove now, both ends are open or closed. */
  RemoveFileOrDir(*m_PipesDir, false);
  write_cmd(fd_control[cli], "PIPE OK\r\n");

  CREATE_NEW_WRITER;

  fd_data[cli] = fd;
}


void cXinelibServer::Handle_Control_DATA(int cli, const char *arg)
{
  LOGDBG("Data connection (TCP) requested");

  CloseDataConnection(cli);

  if(!xc.remote_usetcp) {
    LOGMSG("TCP transports disabled in configuration");
    CloseConnection(cli); /* actually closes the new data connection */
    return;
  }
  
  int clientId = -1, oldId = cli, fd = fd_control[cli];
  if(1 == sscanf(arg, "%d", &clientId) &&
     clientId >= 0 && clientId < MAXCLIENTS &&
     fd_control[clientId] >= 0) {

    CloseDataConnection(clientId);

    fd_control[oldId] = -1;
    cli = clientId;
       
    write_cmd(fd, "DATA\r\n");
    
    CREATE_NEW_WRITER;   

    fd_data[cli] = fd;
    
    /* not anymore control connection, so dec primary device reference counter */
    cXinelibDevice::Instance().ForcePrimaryDevice(false); 

    return;
  }

  LOGDBG("Invalid data connection (TCP) request"); /* closes new data conn., no ctrl conn. */
  CloseConnection(cli);
}

void cXinelibServer::Handle_Control_RTP(int cli, const char *arg)
{
  if(xc.remote_usertp && fd_multicast>=0) {
    char buf[256];
    LOGDBG("Trying RTP connection ...");

    CloseDataConnection(cli);

   sprintf(buf, "RTP %s:%d\r\n", xc.remote_rtp_addr, xc.remote_rtp_port);
    write_cmd(fd_control[cli], buf);

    stream_udp_header_t nullhdr;
    nullhdr.pos = m_StreamPos;
    nullhdr.seq = 0xffff;//-1;//m_UdpSeqNo;

    struct sockaddr_in sin;
    sin.sin_family = sin.sin_family = AF_INET;
    sin.sin_port = sin.sin_port = htons(xc.remote_rtp_port);
    sin.sin_addr.s_addr = inet_addr(xc.remote_rtp_addr);

    if(sizeof(nullhdr) != 
       sendto(fd_multicast, &nullhdr, sizeof(nullhdr), 0, 
	      (struct sockaddr *)&sin, sizeof(sin))) {
      LOGERR("UDP/RTP multicast send() failed");
      //CloseConnection(cli);
      return;
    }

    if(!m_iMulticastMask)
      m_Scheduler->AddHandle(fd_multicast, fd_rtcp); 

    m_bMulticast[cli] = true;
    m_iMulticastMask |= (1<<cli);
    
  } else {
    LOGMSG("RTP transports disabled");
    write_cmd(fd_control[cli], "RTP NONE\r\n");
  }
}

void cXinelibServer::Handle_Control_UDP(int cli, const char *arg)
{
  LOGDBG("Trying UDP connection ...");

  CloseDataConnection(cli);

  if(!xc.remote_useudp) {
    LOGMSG("UDP transports disabled in configuration");
    //CloseConnection(cli);
    return;
  }

  int fd = sock_connect(fd_control[cli], atoi(arg), SOCK_DGRAM);
  if(fd < 0) {
    LOGERR("socket() for UDP failed");
    //CloseConnection(cli);
    return;
  }

  stream_udp_header_t nullhdr;
  nullhdr.pos = m_StreamPos;
  nullhdr.seq = 0xffff;//-1;//m_UdpSeqNo;
  if(sizeof(nullhdr) != send(fd, &nullhdr, sizeof(nullhdr), 0)) {
    LOGERR("UDP send() failed");
    CloseConnection(cli);
    return;
  }

  m_bUdp[cli] = true;
  fd_data[cli] = fd;
  m_Scheduler->AddHandle(fd);
}

void cXinelibServer::Handle_Control_KEY(int cli, const char *arg)
{
  char buf[256], buf2[256];
  bool repeat=false, release=false;

  if(!xc.use_remote_keyboard) {
    LOGMSG("Handle_Control_KEY(%s): Remote keyboard disabled in config", arg);
    return;
  }

  strcpy(buf, arg);
  //*strstr(buf, "\r\n") = 0;
  TRACE("cXinelibServer received KEY " << buf);

  int n = strlen(buf)-1;
  while(buf[n]==' ') buf[n--]=0;

  if(strchr(buf, ' ')) {
    strcpy(buf2,strchr(buf, ' ')+1);
    *strchr(buf, ' ') = 0;
    
    char *pt = strchr(buf, ' ');
    if(pt) {
      *(pt++) = 0;
      if(strstr(pt, "Repeat"))
	repeat = true;
      if(strstr(pt, "Release"))
	release = true;
    }
    cXinelibThread::KeypressHandler(buf, buf2, repeat, release);
  } else {
    cXinelibThread::KeypressHandler(NULL, buf, repeat, release);
  }
}

void cXinelibServer::Handle_Control_CONFIG(int cli)
{
  char buf[256];

  m_bConfigOk[cli] = true;

  int one = 1;
  setsockopt(fd_control[cli], IPPROTO_TCP, TCP_NODELAY, &one, sizeof(int));

  sprintf(buf, "NOVIDEO %d\r\nLIVE %d\r\n", m_bNoVideo?1:0, m_bLiveMode?1:0); 
  write_cmd(fd_control[cli], buf); 

  ConfigureOSD(xc.prescale_osd, xc.unscaled_osd);
  ConfigurePostprocessing(xc.deinterlace_method, xc.audio_delay,
			  xc.audio_compression, xc.audio_equalizer,
                          xc.audio_surround, xc.speaker_type);
  ConfigureVideo(xc.hue, xc.saturation, xc.brightness, xc.contrast, xc.overscan);
  ConfigurePostprocessing("upmix",     xc.audio_upmix ? true : false, NULL);
  ConfigurePostprocessing("autocrop",  xc.autocrop    ? true : false, 
			  xc.AutocropOptions());
#ifdef ENABLE_TEST_POSTPLUGINS
  ConfigurePostprocessing("headphone", xc.headphone   ? true : false, NULL);
#endif

  if(m_bPlayingFile) {
    char buf[2048];
    Unlock();
    int pos = cXinelibDevice::Instance().PlayFileCtrl("GETPOS");
    if(pos<0) pos=0;
    sprintf(buf, "PLAYFILE %d ", pos/1000);
    Lock();
    if(m_bPlayingFile) {
      strcat(buf, m_FileName ? m_FileName : "");
      strcat(buf, "\r\n");
      write_cmd(fd_control[cli], buf);
    }
  }
}

void cXinelibServer::Handle_Control_UDP_RESEND(int cli, const char *arg)
{
  unsigned int seq1, seq2;
  uint64_t pos;

  if( (!fd_data[cli] || !m_bUdp[cli]) &&
      (!m_bMulticast[cli])) {
    LOGMSG("Got invalid re-send request: no udp/rtp in use");
    return;
  }

  if(3 == sscanf(arg, "%d-%d %" PRIu64, &seq1, &seq2, &pos)) {
    
    if(seq1 <= UDP_SEQ_MASK && seq2 <= UDP_SEQ_MASK && pos <= m_StreamPos) {

      if(fd_data[cli] >= 0)
	m_Scheduler->ReSend(fd_data[cli], pos, seq1, seq2);
      else
	m_Scheduler->ReSend(fd_multicast, pos, seq1, seq2);
    } else {
      LOGMSG("Invalid re-send request: %s (send pos=%" PRIu64 ")", 
	     arg, m_StreamPos);
    }
  } else {
    LOGMSG("Invalid re-send request: %s (send pos=%" PRIu64 ")", 
	   arg, m_StreamPos);
  }
}

void cXinelibServer::Handle_Control_GRAB(int cli, const char *arg)
{
  cGrabReplyFuture *f;
  int token = -1, size = 0;
  if(2 == sscanf(arg, "%d %d", &token, &size)) {
    if(size > 0) {
      uchar *result = (uchar*)malloc(size);
      Unlock(); /* may take a while ... */
      ssize_t n = timed_read(fd_control[cli], result, size, 1000);
      Lock();
      if(n == size) {
	if(NULL != (f = (cGrabReplyFuture*)m_Futures->Get(token))) {
	  grab_result_t r;
	  r.Size = size;
	  r.Data = result;
	  m_Futures->Del(f, token);
	  f->Set(r);
	  result = NULL;
	} else {
	  LOGMSG("cXinelibServer: Grab image discarded");
	}
      } else {
	LOGMSG("cXinelibServer: Grab result read() failed");
	CloseConnection(cli);
      }
      free(result);
    } else if(NULL != (f = (cGrabReplyFuture*)m_Futures->Get(token))) {
      grab_result_t r;
      r.Size = 0;
      r.Data = NULL;
      m_Futures->Del(f, token);
      f->Set(r);
    }
  }
}

void cXinelibServer::Handle_Control(int cli, const char *cmd)
{
  TRACEF("cXinelibServer::Handle_Control");

#ifdef LOG_CONTROL_MESSAGES
  static FILE *flog = fopen("/video/control.log","w");
  fprintf(flog,"CTRL (%d): %s\n",cli,cmd); fflush(flog);
#endif

  //LOGDBG("Server received %s", cmd);
  TRACE("Server received " << cmd);

  /* Order of tests is significant !!!
     (example: UDP 2\r\n or UDP FULL 1\r\n) */

  if(!strncasecmp(cmd, "PIPE OPEN", 9)) {
    LOGDBG("Pipe open");

  } else if(!strncasecmp(cmd, "PIPE", 4)) {
    Handle_Control_PIPE(cli, cmd+4);

  } else if(!strncasecmp(cmd, "RTP", 3)) {
    Handle_Control_RTP(cli, cmd+4);

  } else if(!strncasecmp(cmd, "UDP FULL 1", 10)) {
    m_iUdpFlowMask |= (1<<cli);

  } else if(!strncasecmp(cmd, "UDP FULL 0", 10)) {
    m_iUdpFlowMask &= ~(1<<cli);

  } else if(!strncasecmp(cmd, "UDP RESEND ", 11)) {
    Handle_Control_UDP_RESEND(cli, cmd+11);

  } else if(!strncasecmp(cmd, "UDP ", 4)) {
    Handle_Control_UDP(cli, cmd+4);

  } else if(!strncasecmp(cmd, "DATA ", 5)) {
    Handle_Control_DATA(cli, cmd+5);

  } else if(!strncasecmp(cmd, "KEY ", 4)) {
    Handle_Control_KEY(cli, cmd+4);

  } else if(!strncasecmp(cmd, "CONFIG", 6)) {
    Handle_Control_CONFIG(cli);

  } else if(!strncasecmp(cmd, "STC ", 4)) {
    int64_t pts = -1;
    if(1 == sscanf(cmd, "STC %" PRId64, &pts))
      m_StcFuture->Set(pts);

  } else if(!strncasecmp(cmd, "ENDOFSTREAM", 11)) {
    m_bEndOfStreamReached = true;
    
  } else if(!strncasecmp(cmd, "RESULT ", 7)) {
    int token = -1, result = -1;
    if(2 == sscanf(cmd, "RESULT %d %d", &token, &result)) {      
      cReplyFuture *f = m_Futures->Get(token);
      if(f) {
	m_Futures->Del(f, token);
	f->Set(result);
      }
    }

  } else if(!strncasecmp(cmd, "GRAB ", 5)) {
    Handle_Control_GRAB(cli, cmd+5);

  } else if(!strncasecmp(cmd, "CLOSE", 5)) {
    CloseConnection(cli);

  } else if(!strncasecmp(cmd, "GET ", 4)) {
    // HTTP ?
  }
}

void cXinelibServer::Read_Control(int cli)
{
  while(read(fd_control[cli], &m_CtrlBuf[ cli ][ m_CtrlBufPos[cli] ], 1) == 1) {

    ++m_CtrlBufPos[cli];

    if( m_CtrlBufPos[cli] > 79) {
      LOGMSG("Received too long control message from client %d (%d bytes)", 
	     cli, m_CtrlBufPos[cli]);
      LOGMSG("%81s",m_CtrlBuf[cli]);
      CloseConnection(cli);
      return;
    }

    if( m_CtrlBufPos[cli] > 2 &&
	m_CtrlBuf[ cli ][ m_CtrlBufPos[cli] - 2 ] == '\r' &&
	m_CtrlBuf[ cli ][ m_CtrlBufPos[cli] - 1 ] == '\n') {

      m_CtrlBufPos[cli] -= 2;
      m_CtrlBuf[ cli ][ m_CtrlBufPos[cli] ] = 0;

      Handle_Control(cli, m_CtrlBuf[cli]);

      m_CtrlBufPos[cli] = 0;
    }
  }
}

void cXinelibServer::Handle_ClientConnected(int fd)
{
  struct sockaddr_in sin;
  socklen_t len = sizeof(sin);
  char str[1024];
  int cli;

  for(cli=0; cli<MAXCLIENTS; cli++)
    if(fd_control[cli]<0)
      break;

  if(getpeername(fd, (struct sockaddr *)&sin, &len)) {
    LOGERR("getpeername() failed, dropping new incoming connection %d", cli);
    CLOSESOCKET(fd);  
    return;
  }

  uint32_t tmp = ntohl(sin.sin_addr.s_addr);
  LOGMSG("Client %d connected: %d.%d.%d.%d:%d", cli,
	 ((tmp>>24)&0xff), ((tmp>>16)&0xff), 
	 ((tmp>>8)&0xff), ((tmp)&0xff),
	 ntohs(sin.sin_port));

  bool accepted = SVDRPhosts.Acceptable(sin.sin_addr.s_addr);
  if(!accepted) {
    LOGMSG("Address not allowed to connect (svdrphosts.conf).");
    write_cmd(fd, "Access denied.\r\n");
    CLOSESOCKET(fd);  
    return;    
  }

  if(cli>=MAXCLIENTS) {
    // too many clients
    LOGMSG("Too mant clients, connection refused");
    CLOSESOCKET(fd);  
    return;
  }

  if (fcntl (fd, F_SETFL, fcntl (fd, F_GETFL) | O_NONBLOCK) == -1) {
    LOGERR("Error setting control socket to nonblocking mode");
    CLOSESOCKET(fd);
  } 

  CloseDataConnection(cli);

  m_CtrlBufPos[cli] = 0;
  m_CtrlBuf[cli][0] = 0;
    
  sprintf(str,
	  "VDR-" VDRVERSION " "
	  "xineliboutput-" XINELIBOUTPUT_VERSION " "
	  "READY\r\nCLIENT-ID %d\r\n", cli);
  write_cmd(fd, str);
  fd_control[cli] = fd;

  cXinelibDevice::Instance().ForcePrimaryDevice(true);
}

void cXinelibServer::Handle_Discovery_Broadcast()
{
  char buf[1024];
  struct sockaddr_in from;
  socklen_t fromlen = sizeof(from);
  memset(&from, 0, sizeof(from));
  memset(buf, 0, sizeof(buf));
  errno=0;

  int n = recvfrom(fd_discovery, buf, 1023, 0,
		   (struct sockaddr *)&from, &fromlen);

  if(!xc.remote_usebcast) {
    LOGDBG("BROADCASTS disabled in configuration");
    return;
  }

  if(n==0) {
    LOGDBG("fd_discovery recv() 0 bytes");
    return;
  } else if(n<0) {
    LOGERR("fd_discovery recv() error");
    //CLOSESOCKET(fd_discovery);
    return;
  }
	   
  uint32_t tmp = ntohl(from.sin_addr.s_addr);
  LOGDBG("BROADCAST: (%d bytes from %d.%d.%d.%d): %s", n, 
	 ((tmp>>24)&0xff), ((tmp>>16)&0xff), 
	 ((tmp>>8)&0xff), ((tmp)&0xff),
	 buf);

  char *id_string = "VDR xineliboutput DISCOVERY 1.0\r\nClient:";

  if(!strncmp(id_string, buf, strlen(id_string))) {	
    LOGMSG("Received valid discovery message from %d.%d.%d.%d",
	   ((tmp>>24)&0xff), ((tmp>>16)&0xff), 
	   ((tmp>>8)&0xff), ((tmp)&0xff));
    if(udp_discovery_broadcast(fd_discovery, m_Port) < 0) {
      //LOGERR("Discovery broadcast send error");
    } else {
      //LOGMSG("Discovery broadcast (announce) sent");
    }
  }
}

void cXinelibServer::Action(void) 
{

  TRACEF("cXinelibServer::Action");

  int    i, fds=0;
  pollfd pfd[MAXCLIENTS];

  /* higher priority */
  SetPriority(-1); 
  SetPriority(-2);
  SetPriority(-3);

  sched_param temp;
  temp.sched_priority = 2;

  /* request real-time scheduling */
  if (!pthread_setschedparam(pthread_self(), SCHED_RR, &temp)) {
    LOGDBG("cXinelibServer priority set successful SCHED_RR %d [%d,%d]",
	   temp.sched_priority,
	   sched_get_priority_min(SCHED_RR),
	   sched_get_priority_max(SCHED_RR));
  } else {
    LOGDBG("cXinelibServer: Can't set priority to SCHED_RR %d [%d,%d]",
	   temp.sched_priority,
	   sched_get_priority_min(SCHED_RR),
	   sched_get_priority_max(SCHED_RR));
  }
  errno = 0;

  Lock();
  Listen(m_Port);
  m_bReady=true;
  
  if(fd_listen>=0)
    while (!GetStopSignal() && fds>=0) {

      fds = 0;
      if(fd_listen>=0) {
	pfd[fds].fd = fd_listen;
	pfd[fds++].events = POLLIN;
      }
      if(fd_discovery >= 0) {
	pfd[fds].fd = fd_discovery;
	pfd[fds++].events = POLLIN;
      }

      for(i=0; i<MAXCLIENTS; i++) {
        if(fd_control[i]>=0) {
          pfd[fds].fd = fd_control[i];
          pfd[fds++].events = POLLIN;
        }
        if(fd_data[i]>=0) {
          pfd[fds].fd = fd_data[i];
          pfd[fds++].events = 0; /* check for errors only */
        }
      }
      Unlock();

      int err = poll(pfd,fds,1000);
      
      if(err < 0) {
	LOGERR("cXinelibServer: poll failed");
        if(!GetStopSignal()) 
          cCondWait::SleepMs(100); 
	
      } else if(err == 0) {
        // poll timeout
	
      } else {
        Lock();
        for(int f=0; f<fds; f++) {

	  // Check errors (closed connections etc.)
          if(pfd[f].revents & (POLLERR|POLLHUP|POLLNVAL)) {

            if(pfd[f].fd == fd_listen) {
	      LOGERR("cXinelibServer: listen socket error");
              CLOSESOCKET(fd_listen);
	      cCondWait::SleepMs(100);
              Listen(m_Port); 
            } /* fd_listen */

            else if(pfd[f].fd == fd_discovery) {
	      LOGERR("cXinelibServer: discovery socket error");
              CLOSESOCKET(fd_discovery);
            } /* fd_discovery */

            else /* fd_data[] / fd_control[] */ {
	      for(i=0; i<MAXCLIENTS; i++) {
		if(pfd[f].fd == fd_data[i] || pfd[f].fd == fd_control[i]) {
		  LOGMSG("Client %d disconnected", i);
		  CloseConnection(i);
		}
	      }
	    } /* fd_data / fd_control */

          } /* Check ERRORS */

	  // Check ready for reading    
	  else if(pfd[f].revents & POLLIN) {

	    // New connection
            if(pfd[f].fd == fd_listen) {
              int fd = accept(fd_listen, 0, 0);
              if(fd>=0) 
		Handle_ClientConnected(fd);	    
            } /* fd_listen */

	    // VDR Discovery
	    else if(pfd[f].fd == fd_discovery) {
	      Handle_Discovery_Broadcast();
            } /* fd_discovery */

	    // Control data
	    else {
	      for(i=0; i<MAXCLIENTS; i++) {
		if(pfd[f].fd == fd_control[i]) {
		  Read_Control(i);
		  break;
		}		
	      }
	    } /* fd_control */
	      
	  } /* Check ready for reading */

        } /* for(fds) */

        Unlock();
      } /* Check poll result */
      
      Lock();
    } /* while running */
  
  m_bReady = false;
  m_bIsFinished = true;
  Unlock();
}
