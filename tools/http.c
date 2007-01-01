/*
 * http.c: HTTP (/RTSP) helper classes
 *
 * See the main source file 'xineliboutput.c' for copyright information and
 * how to reach the author.
 *
 * $Id: http.c,v 1.1 2007-01-01 00:07:55 phintuka Exp $
 *
 */

#define __STDC_FORMAT_MACROS
#include <inttypes.h>

#include <string.h>

#include <vdr/config.h>
#include <vdr/tools.h>

#include "../logdefs.h"

#include "http.h"

//
// cHttpReq
//

bool cHttpReq::SetCommand(const char *Command) 
{
  char *tmp = strdup(Command);
  char *pt = strchr(tmp, ' '), *uri;

  m_Valid = false;
  if(pt) {
    *pt = 0;
    m_Name = tmp;

    while(*pt && *pt == ' ') pt++;

    uri = pt;
    pt = strrchr(uri, ' ');
    if(pt) {
      m_Version = pt+1;
      while(*pt && *pt == ' ') *pt-- = 0;
      m_Uri = uri;
      m_Valid = true;
    }
  }

  free(tmp);
  return m_Valid;
}

cHeader *cHttpReq::Header(const char *Name) 
{
  for(cHeader *i = m_Headers.First(); i; i = m_Headers.Next(i))
    if(!strcmp(Name, i->Name()))
      return i;
  return NULL;
}

void cHttpReq::AddHeader(const char *Header, bool Duplicate) 
{
  if(strlen(Header) < 4096) {
    char *name = strdup(Header);
    char *val = strchr(name, ':');
    if(val) {
      *val++ = 0;
      AddHeader(name, val, Duplicate);
    }
    free(name);
  } else {
    LOGMSG("cConnState::AddHeader: header length exceeds 4096 !");
  }
}

void cHttpReq::AddHeader(const char *Name, const char *Value, bool Duplicate) 
{
  if(strlen(Name) > 64 || strlen(Value) > 4096) {
    LOGMSG("cConnState::AddHeader: header length exceeds limit !");
  } else {
    cHeader *h = Header(Name);
    if(!Duplicate && h)
      h->SetValue(Value);
    else {
      if(m_Headers.Count() < 50)
	m_Headers.Add(new cHeader(Name, Value));
      else
	LOGMSG("cConnState::AddHeader: header count exceeds 50 !");
    }
  }
}    

void cHttpReq::Reset(void) 
{
  m_Name = NULL;
  m_Uri = NULL;
  m_Version = NULL;
  m_Valid = false;
  m_Headers.Clear();
}

//
// Map file extensions to mime types
//

static const char *mimetype(const char *ext)
{
  static const struct {
    char *ext;
    char *mime;
  } ext2mime[] = {
    {"avi",  "video/avi"},
    {"vob",  "video/mpeg"},
    {"mpg",  "video/mpeg"},
    {"mpeg", "video/mpeg"},
    {"vdr",  "video/mp2p"},
    
    {"mp3",  "audio/mp3"},
    {"flac", "audio/flac"},
    
    {"jpg",  "image/jpeg"},
    {"jpeg", "image/jpeg"},
    {"gif",  "image/gif"},
    
    {NULL, NULL}
  };

  int i = -1;
  while(ext2mime[++i].ext)
    if(!strcmp(ext, ext2mime[i].ext))
      return ext2mime[i].mime;
  return NULL;
}

//
// cHttpStreamer
//

#include "backgroundwriter.h"

cList<cHttpStreamer> cHttpStreamer::m_Streamers;

void cHttpStreamer::CloseAll(bool OnlyFinished)
{
  if(!OnlyFinished) {
    while(m_Streamers.First())
      m_Streamers.Del(m_Streamers.First());

  } else {
    /*  purge finished streamers from list */
    cHttpStreamer *it = m_Streamers.First();
    while(it) {
      if(it->Active()) {
	it = (cHttpStreamer*)it->Next();
      } else {
	m_Streamers.Del(it);
	it = m_Streamers.First();
      }
    }
  }
}

cHttpStreamer::cHttpStreamer(int fd_http, const char *filename, const char *Range)
{
  m_fds.set_handle(fd_http);
  m_fdf = -1;

  m_Filename = filename;
  m_FileSize = -1;
  m_Start = 0;
  m_End = -1;

  m_ConnState = NULL;

  m_Finished = false;

  CloseAll(true);

  m_Writer = new cBackgroundWriter(fd_http, KILOBYTE(128), true);
  m_Streamers.Add(this);

  if(m_Streamers.Count() > 5) {
    LOGMSG("WARNING: There are %d running HTTP streamers !", m_Streamers.Count());
    if(m_Streamers.Count() > 20) {
      errno = 0;
      LOGERR("ERROR: There are %d running HTTP streamers, cancelling first", 
	     m_Streamers.Count());
      m_Streamers.Del(m_Streamers.First());
    }
  }

  ParseRange(Range);

  Start();
}

cHttpStreamer::~cHttpStreamer()
{
  Cancel(3);
  if(m_Writer)
    delete m_Writer;
  if(m_ConnState)
    delete m_ConnState;
  if(m_fdf >= 0) 
    close(m_fdf);
  m_fdf = -1;
}

void cHttpStreamer::ParseRange(const char *Range)
{
  m_Start = 0;
  m_End = -1;
  if(Range) {
    LOGMSG("Range: %s", Range);
    switch(sscanf(Range, "%" PRId64 "-%" PRId64, &m_Start, &m_End)) {
    case 2: LOGMSG("Range: %s (%" PRId64 " - %" PRId64 ")", Range, m_Start, m_End);
            break;
    case 1: m_End = -1;
            LOGMSG("Range: %s (%" PRId64 " -)", Range, m_Start);
    case 0: m_Start = 0;
    default: break;
    }
  }
}

bool cHttpStreamer::Seek(void)
{
  if(m_fdf < 0) {
    m_fdf = open(m_Filename, O_RDONLY);
    if(m_fdf < 0) {
      LOGERR("cHttpStreamer: error opening %s", *m_Filename);
      
      m_fds.write_cmd("HTTP/1.1 401 Not Found\r\n");
      m_fds.write_cmd("Connection: Close\r\n");
      m_fds.write_cmd("\r\n");
      return false;
    }
  }

  off_t FileSize = lseek(m_fdf, 0, SEEK_END);
  if(m_Start >= FileSize)
    m_Start = FileSize - 1;

  if(m_Start > 0) {
    if(m_End >= FileSize)
      m_End = FileSize - 1;

    m_fds.write_cmd("HTTP/1.1 206 Partial Content\r\n");
    if(m_End <= 0)
      m_fds.printf("Range: %" PRId64 "-\r\n", m_Start);
    else
      m_fds.printf("Range: %" PRId64 "-%" PRId64 "\r\n", m_Start, m_End);
  } else {
    m_fds.write_cmd("HTTP/1.1 200 OK\r\n");
  }

  /* content type */
  char *ext = strrchr(m_Filename, '.');
  if(ext) {
    const char *mime = mimetype(ext+1);
    if(mime)
      m_fds.printf("Content-Type: %s\r\n", mime);
  }

  /* Content-Length */
  if(m_FileSize >= 0) {
    int64_t len = m_FileSize;
    if(m_End >= 0)
      len = m_End + 1;
    else if(m_Start >= 0)
      len -= m_Start;
    m_fds.printf("Content-Length: %" PRId64 "\r\n", len);
  }

  /* Connection */
  m_fds.write_cmd("Connection: Keep-Alive\r\n");

  /* End of reply */
  m_fds.write_cmd("\r\n");

  if(m_Start)
    lseek(m_fdf, (off_t)m_Start, SEEK_SET);
  else
    lseek(m_fdf, 0, SEEK_SET);

  return true;
}

bool cHttpStreamer::ReadPipelined(void)
{
  char buf[2048];
  int r;

  if(m_ConnState)
    delete m_ConnState;
  m_ConnState = new cConnState;

  do {
    r = m_fds.readline(buf, sizeof(buf), 100);
    if(r < 0 || errno == EAGAIN || r >= (int)sizeof(buf))
      return false;

    LOGMSG("HTTP pipelined: %s", buf);

    if(!*m_ConnState->Name()) {
      if(!m_ConnState->SetCommand(buf) ||
	 !strcmp(m_ConnState->Name(), "GET") ||
	 !strncmp(m_ConnState->Uri(), "/PLAYFILE", 9)) {
	LOGMSG("Incorrect HTTP request: %s", buf);
	return false;
      }
    }
    else if(r > 0)
      m_ConnState->AddHeader(buf);
  } while(r>0);

  if(m_ConnState->Header("Range"))
    ParseRange(m_ConnState->Header("Range")->Value());

  return true;
}

void cHttpStreamer::Action(void)
{
  // not implemented

  m_Finished = true;
}
