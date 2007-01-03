/*
 * playlist.c: Media player playlist
 *
 * See the main source file 'xineliboutput.c' for copyright information and
 * how to reach the author.
 *
 * $Id: playlist.c,v 1.3 2007-01-03 19:12:18 phintuka Exp $
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

#define MAX_PLAYLIST_FILES 1024


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
  if(Position != o->Position) 
    return Position > o->Position ? 1 : -1;

  // same position (or no positions definend) -> alphabetical order
#if 0
  return strcmp(Track, o->Track);
#else
  // use filename, because:
  //  - implicit playlist has no track names available when sorting 
  //    (track names are read during playback), so track name is 
  //    just file name without path.
  //    using full path allows sorting of each album and tracks inside albums...
  //  - "normal" playlist is ordered using Position,
  //     so track names are never compared anyway ...
  return strcmp(Filename, o->Filename);
#endif
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

    (void)nice(10);
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
	    if(!strncmp(pt, "Artist: ", 8) && strlen(pt) > 9)
	      Item->Artist = (pt+8);
	    if(!strncmp(pt, "Album: ", 7) && strlen(pt) > 8)
	      Item->Album = (pt+7);
	    if(!strncmp(pt, "Track: ", 7) && strlen(pt) > 8)
	      Item->Track = (pt+7);
	  }
	}
      }
    }
    LOGDBG("ID3Scanner Done.");

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
	if(*(t+1))
	  return t+1;
      }
      else if(!strncasecmp(line, "title", 5) && 
	      1 == sscanf(line + 5, "%d=", &n)) {
	if(*(t+1)) {
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

void cPlaylist::Del(cPlaylistItem *it)
{
  cMutexLock ml(&m_Lock);

  if(!it || Count() < 2)
    return;

  if(m_Current == it)
    m_Current = cList<cPlaylistItem>::Next(Current()) ?:
		cList<cPlaylistItem>::Prev(Current());

  cListBase::Del(it);
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
	if(*i->Track && (*i->Track)[0])
	  fprintf(f, "Title%d=%s\r\n", entries, *i->Track);
	if(*i->Artist && (*i->Artist)[0])
	  fprintf(f, "Artist%d=%s\r\n", entries, *i->Artist);
	if(*i->Album && (*i->Album)[0])
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

static const char *strchrnext(const char *s, char c)
{
  return (s = strchr(s, c)) ? ((*(s+1))?(s+1):NULL) : NULL;
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
	  const char *Filename = strchrnext(pt+4, '=');
	  if(Filename && *Filename) {
	    for(cPlaylistItem *i = First(); i; i=Next(i)) {
	      if(!strncmp(i->Filename, m_Folder, len)) {
		if(!strcmp(*i->Filename + len, Filename)) {
		  it = i;
		  break;
		}
	      }
	    }
	  }
	} else if(it && !strncmp(pt, "Title", 5)) {
	  it->Track = strchrnext(pt, '=');
	} else if(it && !strncmp(pt, "Artist", 6)) {
	  it->Artist = strchrnext(pt, '=');
	} else if(it && !strncmp(pt, "Album", 5)) {
	  it->Album = strchrnext(pt, '=');
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
	      if(n<MAX_PLAYLIST_FILES) {
		depth++; /* limit depth */
		Buffer = cString::sprintf("%s/", *Buffer);
		n += ScanFolder(Buffer, Recursive, Filter);
		depth--;
	      } else {
		if(!++warn)
		  LOGMSG("ScanFolder: Found over %d matching files, list truncated!", n);
		break;
	      }
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
	    /* TODO: Should ScanDir add contents of playlist files ... ? */
	    if(Filter == &config_t::IsPlaylistFile || !xc.IsPlaylistFile(Buffer)) {
	      n++;
	      if(n<MAX_PLAYLIST_FILES) {
		Add(new cPlaylistItem(e->d_name, FolderName));
		//LOGDBG("ScanFolder: %s", e->d_name);
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
    LOGDBG("ScanFolder: Found %d matching files from %s", n, FolderName);
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
      //LOGDBG("ID3 Cache modified %d, folder modified %d, diff %d",
      //       (unsigned int)stc.st_mtime, (unsigned int)stf.st_mtime, 
      //       (unsigned int)(stc.st_mtime - stf.st_mtime));
      if(stc.st_mtime >= stf.st_mtime) {
	if(ReadCache()) {
	  LOGDBG("cPlaylist: using up-to-date ID3 cache");
	  //LOGMSG("  Cache read OK.");
	  return;
	}
	LOGMSG("cPlaylist: ID3 cache read FAILED");
      } else {
	LOGDBG("cPlaylist: ID3 cache not up-to-date, using old cache and scanning for changes");
	ReadCache();
      }
    }
    //else LOGERR("cPlaylist: stat(%s) failed");
  }
  //else LOGERR("cPlaylist: stat(%s) failed");

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
    LOGDBG("cPlaylist: fetching remote playlist from %s", file);
    cString Cmd = cString::sprintf("curl %s", file);
    if(!p.Open(Cmd, "r")) {
      LOGERR("cPlaylist: CURL command (%s) failed", *Cmd);
      return false;
    }
    // process as normal file
    f = p;
  }
  
  if(f) {
    LOGDBG("cPlaylist: parsing %s", file);
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

	if(depth && n==0) {
	  // TODO
	  //  - add "separator" item
	  // Add(new cPlaylistItem(NULL, Base, "---");
	}

	if(xc.IsPlaylistFile(pt)) {
	  parser->ResetCache();
	  LOGDBG("cPlaylist: found playlist inside playlist");
	  if(depth > 4)
	    LOGMSG("cPlaylist: recursion too deep, skipped %s", pt);
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
	  //LOGDBG("read_playlist: %s", pt);
	  n++;
	}
      }
    }

    if(! (FILE*) p)
      fclose(f);

    if(n >= MAX_PLAYLIST_FILES) 
      LOGMSG("cPlaylist: Found over %d matching files, list truncated!", n);
    LOGDBG("cPlaylist: Found %d matching files", n);
    return n;
  }

  LOGERR("cPlaylist: Error opening %s", file);
  return 0;
}

static cString LastDir(cString& path)
{
  cString tmp = strdup(path);
  char *pt = strrchr(tmp, '/');
  if(pt && pt > *tmp) {
    *pt = 0;
    pt = strrchr(tmp, '/');
    if(pt)
      return cString(pt+1);
  }
  return cString(NULL);
}

bool cPlaylist::Read(const char *PlaylistFile, bool Recursive) 
{
  cMutexLock ml(&m_Lock);
  bool Result = true;

  // extract playlist root folder
  m_Folder = PlaylistFile;
  if(strrchr(m_Folder, '/'))
    *(strrchr(m_Folder, '/') + 1) = 0;


  if(xc.IsPlaylistFile(PlaylistFile)) {
    // Read playlist file
    Result = ReadPlaylist(PlaylistFile);
    m_Origin = ePlaylist;

  } else if(PlaylistFile[strlen(PlaylistFile)-1] == '/') {
    // Scan folder
    Result = ScanFolder(PlaylistFile, Recursive) > 0;
    m_Origin = eImplicit;
    Sort();

    if(!*m_Name) {
      m_Name = PlaylistFile;
      *(strrchr(m_Name, '/')) = 0;
      if(strrchr(m_Name, '/')) {
	cString dir = LastDir(m_Name);
	if(*dir)
	  m_Name = cString::sprintf("%s - %s", *dir, strrchr(m_Name, '/')+1);
	else
	  m_Name = strrchr(m_Name, '/')+1;
      }
    }

  } else {
    // Single file
    Add(new cPlaylistItem(PlaylistFile));
    m_Origin = eImplicit;
  }

  if(!*m_Name) {
    char *pt = strrchr(PlaylistFile, '/');
    pt = pt ? pt+1 : NULL;

    cString dir = LastDir(m_Folder);
    if(*dir && pt)
      m_Name = cString::sprintf("%s - %s", *dir, pt ?: "");
    else
      m_Name = pt ?: "";

    if(strrchr(m_Name, '.'))
      *(strrchr(m_Name, '.')) = 0;
  }

  if(Count() < 1) {
    LOGMSG("Empty playlist %s !", PlaylistFile);
    Add(new cPlaylistItem(PlaylistFile));
  }
    
  return Result;
}



