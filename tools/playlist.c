/*
 * playlist.c: Media player playlist
 *
 * See the main source file 'xineliboutput.c' for copyright information and
 * how to reach the author.
 *
 * $Id: playlist.c,v 1.1 2006-12-24 16:01:28 phintuka Exp $
 *
 */

#include <stdlib.h> 

#include <vdr/config.h> 
#include <vdr/tools.h> 
#include <vdr/thread.h> 

#include "../config.h"

#include "playlist.h"

#include "../logdefs.h"


#ifndef PLAYLIST_CACHE
#  define PLAYLIST_CACHE ".xineliboutput-playlist.pls"
#endif

#define MAX_PLAYLIST_FILES  256


//
// cPlaylistItem
//

cPlaylistItem::cPlaylistItem(const char *filename)
{
  char *pt;

  Filename = filename;
  Position = -1;

  if(NULL != (pt = strrchr(filename, '/')))
    Track = pt + 1;
  else
    Track = filename;
  
  if(NULL != (pt = strrchr(Track, '.')))
    *pt = 0;
}

cPlaylistItem::cPlaylistItem(const char *filename, 
			     const char *path,
			     const char *title, 
			     int position)
{
  char *pt;

  if(path[strlen(path)-1] != '/')
    Filename = cString::sprintf("%s/%s", path, filename);
  else
    Filename = cString::sprintf("%s%s", path, filename);
  Position = position;
  Track = title ?: filename;

  if(!title && (pt = strrchr(Track, '.')))
    *pt = 0;
}

int cPlaylistItem::Compare(const cListObject &ListObject) const 
{
  ///< Must return 0 if this object is equal to ListObject, a positive value
  ///< if it is "greater", and a negative value if it is "smaller".

  const cPlaylistItem *o = (cPlaylistItem *)&ListObject;

  // Use Position (if defined in playlist file)
  if(Position != o->Position) {
    if(Position == -1)
      return -1;
    if(o->Position == -1)
      return 1;
    return Position > o->Position ? 1 : -1;
  }

  /* same position or no positions definend -> alphabetical order */
  return strcmp(Track, o->Track);
}


//
// cID3Scanner
//

class cID3Scanner : public cThread 
{
 public:
  cPlaylist& m_List;
  cID3Scanner(cPlaylist& List) : m_List(List), m_Done(false) {};
  
  void CancelScanner(void) { Cancel(3); }

 private:
  bool m_Done;

  virtual void Action(void) 
  {
    cPlaylistItem *Item = NULL;

    nice(10);
    cCondWait::SleepMs(5000);

    LOGDBG("ID3Scanner Started");
    while(Running()) {
      if(!(Item = m_List.Next(Item)))
	break;
      // id3 tags can be in other files too (eg. flac)
      /*if(!strcasecmp((Item->Filename) + strlen(Item->Filename) - 4, ".mp3")) {*/
      if(xc.IsAudioFile(Item->Filename)) {
	cString Cmd = cString::sprintf("mp3info -p \""
				       "Artist: %%a\\r\\n"
				       "Album: %%l\\r\\n"
				       "Track: %%t\\r\\n\" \'%s\'",
				       *Item->Filename);
	cPipe p;
	if(p.Open(*Cmd, "r")) {
	  cMutexLock ml(&m_List.m_Lock);
	  cReadLine r;
	  char *pt;
	  while(NULL != (pt = r.Read(p))) {
	    if(!strncmp(pt, "Artist: ", 8))
	      Item->Artist = (pt+8);
	    if(!strncmp(pt, "Album: ", 7))
	      Item->Album = (pt+7);
	    if(!strncmp(pt, "Track: ", 7))
	      Item->Track = (pt+7);
	  }
	}
      }
    }
    LOGDBG("ID3Scanner: Done.");

    m_List.PlaylistChanged(Item);
    m_Done = true;
  }
};

//
// cPlaylistReader
//

class cPlaylistReader 
{
 private:
  cPlaylist& m_Playlist;

 protected:
  cString    m_Title;
  int        m_Position;

  cPlaylistItem *Prev(void) { return m_Playlist.Last(); }

 public:
  cPlaylistReader(cPlaylist& Playlist) : m_Playlist(Playlist) {}
  virtual ~cPlaylistReader() {}

  virtual char *Parse(char *line) = 0;

  void ResetCache(void) { m_Title = NULL; m_Position = -1; }
  const char *Title(void) { return m_Title; }
  int Position(void) { return m_Position; }
};

class cM3uReader : public cPlaylistReader
{
 public:
  cM3uReader(cPlaylist& Playlist) : cPlaylistReader(Playlist), m_Next(1) {}

 protected:
  int m_Next;
  virtual char *Parse(char *line) {
    if(!*line)
      return NULL;
    if(*line == '#') {
      if(!strncmp(line, "#EXTINF:", 8)) {
	int len = -1;
	sscanf(line+8,"%d", &len);
	while(*line && *line != ',')
	  line++;
	m_Title = *line ? (line+1) : NULL;
	m_Position = m_Next++;
      }
      return NULL;
    }
    return *line ? line : NULL;
  }
};

class cPlsReader : public cPlaylistReader
{
 public:
  cPlsReader(cPlaylist& Playlist) : cPlaylistReader(Playlist), m_Current(0) {}

 protected:
  int  m_Current;
  virtual char *Parse(char *line) {
    char *t = strchr(line, '=');  
    if(t) {
      int n;
      if(!strncasecmp(line, "file", 4) && 
	 1 == sscanf(line + 4, "%d=", &n)) {
	m_Current = n;
	m_Position = n;
	if(t && *(t+1))
	  return t+1;
      }
      else if(!strncasecmp(line, "title", 5) && 
	      1 == sscanf(line + 5, "%d=", &n)) {
	if(t++) {
	  if(n == m_Current)
	    Prev()->Track = t;
	  else
	    m_Title = t;
	}
      }
      //else if(!strncasecmp(line, "length", 6) && 
      //        1 == sscanf(line + 4, "%d=", &n)) {
      //}
    }
    return NULL;
  }
};

class cAsxReader : public cPlaylistReader
{
 public:
  cAsxReader(cPlaylist& Playlist) : cPlaylistReader(Playlist) {}

 protected:
  virtual char *Parse(char *line) {
    char *pt = strstr(line, "<REF HREF");
    if(!pt)
      pt = strstr(line, "<ref href");
    if(!pt)
      pt = strstr(line, "<ENTRY HREF");
    if(!pt)
      pt = strstr(line, "<entry href");
    if(pt) {
      pt = strchr(pt, '=');
      if(pt) {
	pt = strchr(pt, '\"');
	if(pt) {
	  pt++;
	  if(strchr(pt, '\"'))
	    *strchr(pt, '\"') = 0;
	  return pt;
	}
      }
    }

    pt = strstr(line, "<TITLE>");
    if(!pt)
      pt = strstr(line, "<title>");
    if(pt) {
      pt += 7;
      if(strstr(line, "</"))
	*strstr(line, "</") = 0;
      m_Title = pt;
    }

    if(*m_Title) {
      pt = strstr(line, "<ENTRY>");
      if(!pt)
	pt = strstr(line, "<entry>");
      if(pt) {
	if(*m_Title && Prev()) {
	  Prev()->Track = m_Title;
	  m_Title = NULL;
	}
      }
    }
    return NULL;
  }
};


//
// cPlaylist
//

cPlaylist::cPlaylist()
{
  m_Origin  = eImplicit;
  m_Menu    = NULL;
  m_Scanner = NULL;
  m_Current = NULL;
}

cPlaylist::~cPlaylist()
{
  if(m_Scanner) {
    m_Scanner->CancelScanner();
    delete m_Scanner;
  }

  if(m_Origin == eImplicit)
    StoreCache();
}

void cPlaylist::Listen(cPlaylistChangeNotify *Menu)
{
  cMutexLock ml(&m_Lock);
  m_Menu = Menu;
}

void cPlaylist::PlaylistChanged(const cPlaylistItem *Item)
{
  cMutexLock ml(&m_Lock);
  if(m_Menu)
    m_Menu->PlaylistChanged(Item);
}

void cPlaylist::Sort(void) 
{
  cMutexLock ml(&m_Lock);
  cListBase::Sort(); 
}

int cPlaylist::Count(void) const 
{
  return cListBase::Count(); 
}

cPlaylistItem *cPlaylist::Next(const cPlaylistItem *i)
{
  cMutexLock ml(&m_Lock);
  return i ? cList<cPlaylistItem>::Next(i) : cList<cPlaylistItem>::First(); 
}

cPlaylistItem *cPlaylist::Current(void)
{
  cMutexLock ml(&m_Lock);
  return m_Current ?: First(); 
}

void cPlaylist::SetCurrent(cPlaylistItem *current) 
{
  cMutexLock ml(&m_Lock);
  m_Current = current; 
}

cPlaylistItem *cPlaylist::Next(void) 
{
  cMutexLock ml(&m_Lock);
  if(Current())
    return m_Current = (cList<cPlaylistItem>::Next(Current()) ?: First());
  return NULL;
}

cPlaylistItem *cPlaylist::Prev(void) 
{
  cMutexLock ml(&m_Lock);
  if(Current())
    return m_Current = (cList<cPlaylistItem>::Prev(Current()) ?: Last());
  return NULL;
}

bool cPlaylist::StoreCache(void) 
{
  if(!xc.cache_implicit_playlists ||
     m_Origin != eImplicit ||
     !*m_Folder)
    return false;

  cString Name = cString::sprintf("%s%s", *m_Folder, PLAYLIST_CACHE);
  int len = strlen(m_Folder), entries = 0;
  FILE *f = NULL;

  for(cPlaylistItem *i = First(); i; i=Next(i)) {
    // store only items in "current" root folder
    if(!strncmp(i->Filename, m_Folder, len)) {
      if(/**i->Track ||*/ *i->Artist || *i->Album) {
	cString Filename = ((*i->Filename) + len); // relative
	if(entries < 1) {
	  f = fopen(Name, "w");
	  if(!f) {
	    LOGERR("creation of metadata cache %s%s failed", 
		   *m_Folder, PLAYLIST_CACHE);
	    return false;
	  }
	  fprintf(f, "[playlist]\r\n");
	}
	entries++;
	fprintf(f, "File%d=%s\r\n", entries, *Filename);
	fprintf(f, "Title%d=%s\r\n", entries, *i->Track);
	if(*i->Artist)
	  fprintf(f, "Artist%d=%s\r\n", entries, *i->Artist);
	if(*i->Album)
	  fprintf(f, "Album%d=%s\r\n", entries, *i->Album);
      }
    }
  }
  
  if(entries > 0) {
    fprintf(f, "NumberOfEntries=%d\r\nVersion=2\r\n", entries);
    fclose(f);
    return true;
  }

  return false; 
}

bool cPlaylist::ReadCache(void) 
{
  if(m_Origin == eImplicit && *m_Folder) {

    cString Name = cString::sprintf("%s%s", *m_Folder, PLAYLIST_CACHE);
    FILE *f = fopen(Name, "r");
    if(f) {
      int len = strlen(m_Folder);
      cPlaylistItem *it = NULL;
      cReadLine r;
      char *pt;
      while(NULL != (pt = r.Read(f))) {
	if(!strncmp(pt, "File", 4)) {
	  it = NULL;
	  cString Filename = strchr(pt, '=');
	  for(cPlaylistItem *i = First(); i; i=Next(i)) {
	    if(!strncmp(i->Filename, m_Folder, len)) {
	      if(!strcmp(*i->Filename + len, Filename)) {
		it = i;
		break;
	      }
	    }
	  }
	} else if(it && !strncmp(pt, "Title", 5)) {
	  it->Track = strchr(pt, '=');
LOGMSG("ReadCache: Track  -> %s", *it->Track);
	} else if(it && !strncmp(pt, "Artist", 6)) {
	  it->Artist = strchr(pt, '=');
LOGMSG("ReadCache: Artist -> %s", *it->Artist);
	} else if(it && !strncmp(pt, "Album", 5)) {
	  it->Album = strchr(pt, '=');
LOGMSG("ReadCache: Album  -> %s", *it->Album);
	} else {
	  /*it = NULL;*/
	}
      }
      fclose(f);
      return true;
    }
  }

  return false; 
}

#if 0
static FILE *open_http(const char *PlaylistFile)
{
  char file[1024] = "", host[128] = "", pt;
  int  fd, port = 80;

  strn0cpy(host, PlaylistFile+strlen("http://"), sizeof(host)-1);
  pt = strchr(host, '/');
  if(pt) {
    strn0cpy(file, pt, sizeof(file)-1);
    *pt = 0;
  }
  pt = strchr(host, ':');
  if(pt) {
    *pt++ = 0;
    port = atoi(pt);
  }

  fd = tcp_connect(host, port);
  if(fd < 0) {
    LOGERR("TCP connect failed");
    return NULL;
  }

  int len = asprintf(&pt, 
		     "GET %s HTTP/1.1"  "\r\n"
		     "Host: %s"         "\r\n"
		     "\r\n",
		     file, host);
  if(len != write(fd, pt, len)) {
    LOGERR("HTTP request write failed");
    free(pt);
    close(fd);
    return NULL;
  }
  free(pt);

  int state = 0;
  FILE *f = fdopen(fd, "r");
  cReadLine r;
  while(state >= 0 && NULL != (pt = r.Read(f))) {
    switch(state) {
      case 0: if(!strncmp(pt, "HTTP/1", 6) || !strstr(pt, " 200 ")) {
	        LOGERR("HTTP error: %s", pt);
		fclose(f);
		return NULL;
              }
              state = 1;
              break;
      case 1: if(strcmp(pt, "\r\n")) 
	        break;
              return f;
      default: break;
    }
  }

  fclose(f);
  return NULL;
}
#endif

int cPlaylist::ScanFolder(const char *FolderName, 
			  bool Recursive,
			  bool (config_t::*Filter)(const char *))
{
  cMutexLock ml(&m_Lock);  
  static int depth = 0;
  DIR *d = opendir(FolderName);

  if (d) {
    LOGDBG("ScanFolder(%s)", FolderName);
    struct dirent *e;
    int n = 0, warn = -1;
    while ((e = readdir(d)) != NULL) {
      cString Buffer = cString::sprintf("%s%s", FolderName, e->d_name);
      struct stat st;
      if (stat(Buffer, &st) == 0) {
	if(S_ISDIR(st.st_mode)) {
	  if (Recursive && !S_ISLNK(st.st_mode)) { /* don't want to loop ... */
	    if(depth > 4) {
	      LOGMSG("ScanFolder: Too deep directory tree");
	    } else if(e->d_name[0]=='.') {
	    } else {
	      depth++; /* limit depth */
	      Buffer = cString::sprintf("%s/", *Buffer);
	      n += ScanFolder(Buffer);
	      depth--;
	    }
	  }
	} else /* == if(!S_ISDIR(st.st_mode))*/ {
	  // check symlink destination
	  if (S_ISLNK(st.st_mode)) {
	    Buffer = ReadLink(Buffer);
	    if (!*Buffer)
	      continue;
	    if (stat(Buffer, &st) != 0)
	      continue;
	  }
	  if((xc.*Filter)(Buffer)) {
	    if(Filter == &config_t::IsPlaylistFile || !xc.IsPlaylistFile(Buffer)) {
	      n++;
	      if(n<MAX_PLAYLIST_FILES) {
		Add(new cPlaylistItem(e->d_name, FolderName));
		LOGDBG("ScanFolder: %s", e->d_name);
	      } else {
		if(!++warn)
		  LOGMSG("ScanFolder: Found over %d matching files, list truncated!", n);
		break;
	      }
	    }
	  }
	}
      }
    }
    LOGDBG("ScanFolder: Found %d matching files", n);
    closedir(d);

    return n;
  }

  LOGERR("ScanFolder: Error opening %s", FolderName);
  return 0;
}

void cPlaylist::StartScanner(void)
{
  cMutexLock ml(&m_Lock);

  if(m_Scanner) {
    if(m_Scanner->Active())
      return;
    delete m_Scanner;
    m_Scanner = NULL;
  }

  /* check if cache is already up-to-date */
  cString CacheName = cString::sprintf("%s%s", *m_Folder, PLAYLIST_CACHE);
  struct stat stf, stc;
  if(!stat(m_Folder, &stf)) {
    if(!stat(CacheName, &stc)) {
      LOGMSG("Cache modified: %d, folder modified: %d, diff %d",
	     (unsigned int)stc.st_mtime, (unsigned int)stf.st_mtime, 
	     (unsigned int)(stc.st_mtime - stf.st_mtime));
      if(stc.st_mtime < stf.st_mtime) {
	LOGMSG("  -> using up-to-date cache");
	if(ReadCache()) {
	  LOGMSG("  Cache read OK.");
	  return;
	}
	LOGMSG("  Cache read FAILED");
      } else {
	LOGMSG("  -> using cache and scanning for changes");
	ReadCache();
      }
    } else 
      LOGERR("stat(Cache) failed");
  } else
    LOGERR("stat(Folder) failed");

  if(xc.enable_id3_scanner) {
    m_Scanner = new cID3Scanner(*this);
    m_Scanner->Start();
  }
}

int cPlaylist::ReadPlaylist(const char *file)
{
  static int depth = 0; /* limit recursion */
  cPipe p;
  cPlaylistReader *parser = NULL;
  FILE *f;

  if(strncmp(file, "http:", 5) && strncmp(file, "https:", 6)) {
    f = fopen(file, "r");
  } else {
    // fetch playlist from server using curl
    cString Cmd = cString::sprintf("curl %s", file);
    if(!p.Open(Cmd, "r")) {
      LOGERR("CURL command (%s) failed", *Cmd);
      return false;
    }
    // process as normal file
    f = p;
  }
  
  if(f) {
    LOGDBG("parse_playlist(%s)", file);
    char *pt = strrchr(file, '.');
    if(!strcasecmp(pt, ".pls"))
      parser = new cPlsReader(*this);
    else if(!strcasecmp(pt, ".asx"))
      parser = new cAsxReader(*this);
    else /*if(!strcasecmp(pt, ".m3u"))*/
      parser = new cM3uReader(*this); /* parses plain lists (.ram, ...) too ...*/

    cString Base(file);
    if(NULL != (pt=strrchr(Base,'/')))
      pt[1]=0;

    int n = 0;
    cReadLine r;
    while(NULL != (pt = r.Read(f)) && n < MAX_PLAYLIST_FILES) {
      if(NULL != (pt = parser->Parse(pt))) {

	if(xc.IsPlaylistFile(pt)) {
	  parser->ResetCache();
	  LOGMSG("playlist inside playlist");
	  if(depth > 4)
	    LOGMSG("recursion too deep, skipped %s", pt);
	  else {
	    depth++;
	    n += ReadPlaylist(pt);
	    depth--;
	  }

	} else {
	  if(*pt == '/' || 
	     (strstr(pt,"://")+1 == strchr(pt,'/') && 
	      strchr(pt,'/') - pt < 8)) {
	    // absolute path
	    Add(new cPlaylistItem(pt));
	    if(parser->Title())
	      Last()->Track = parser->Title();
	  } else {
	    // relative path
	    Add(new cPlaylistItem(pt, Base, parser->Title()));
	  }
	  Last()->Position = parser->Position();
	  parser->ResetCache();
	  LOGDBG("read_playlist: %s", pt);
	  n++;
	}
      }
    }

    if(! (FILE*) p)
      fclose(f);

    if(n >= MAX_PLAYLIST_FILES) 
      LOGMSG("read_playlist: Found over %d matching files, list truncated!", n);
    LOGDBG("read_playlist: Found %d matching files", n);
    return n;
  }

  LOGERR("read_playlist: Error opening %s", file);
  return 0;
}

bool cPlaylist::Read(const char *PlaylistFile, bool Recursive) 
{
  cMutexLock ml(&m_Lock);
  bool Result = true;

  m_Folder = PlaylistFile;

  if(xc.IsPlaylistFile(PlaylistFile)) {
    // Playlist file
    char *pt = strrchr(PlaylistFile, '/');
    m_Name = pt ? pt+1 : "";
    *(strrchr(m_Name, '.')) = 0;
    if(strrchr(m_Folder, '/'))
      *(strrchr(m_Folder, '/') + 1) = 0;

    Result = ReadPlaylist(PlaylistFile);
    m_Origin = ePlaylist;

  } else if(PlaylistFile[strlen(PlaylistFile)-1] == '/') {
    // Whole folder
    m_Name = PlaylistFile;
    if(strrchr(m_Name, '/'))
      *(strrchr(m_Name, '/')) = 0;
    if(strrchr(m_Name, '/'))
      m_Name = strrchr(m_Name, '/')+1;

    Result = ScanFolder(PlaylistFile, Recursive) > 0;
    m_Origin = eImplicit;

    Sort();

  } else {
    // Single file
    char *pt = strrchr(PlaylistFile, '/');
    m_Name = pt;
    if(strrchr(m_Name, '.'))
      *(strrchr(m_Name, '.')) = 0;
    if(strrchr(m_Folder, '/'))
      *(strrchr(m_Folder, '/') + 1) = 0;

    Add(new cPlaylistItem(PlaylistFile));
    m_Origin = eImplicit;
  }

  if(Count() < 1) {
    LOGMSG("Empty playlist %s !", PlaylistFile);
    Add(new cPlaylistItem(PlaylistFile));
  }
    
  return Result;
}



