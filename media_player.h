/*
 * media_player.h: Media and image players
 *
 * See the main source file 'xineliboutput.c' for copyright information and
 * how to reach the author.
 *
 * $Id: media_player.h,v 1.21 2010-12-09 13:24:55 phintuka Exp $
 *
 */

#ifndef __XINELIB_PLAYER_H
#define __XINELIB_PLAYER_H

// --- Image player ---------------------------------------------------------

#include <vdr/player.h>

class cXinelibImagePlayer;

class cXinelibImagesControl : public cControl 
{
  private:
    static cXinelibImagePlayer *m_Player;
    static cMutex m_Lock;

    cSkinDisplayReplay *m_DisplayReplay;

    char **m_Files;
    char *m_File;
    int m_Index;
    int m_Count;
    int m_Speed;
    int m_LastShowTime;
    bool m_ShowModeOnly;

    static cXinelibImagePlayer *OpenPlayer(const char *File);

  protected:
    void Seek(int Rel);
    void Delete(void);

  public:
    cXinelibImagesControl(char **Files, int Index, int Count);
    virtual ~cXinelibImagesControl();

    virtual void Show(void);
    virtual void Hide(void);
    virtual eOSState ProcessKey(eKeys Key);

    virtual cOsdObject *GetInfo(void);

    static void Close(void);
    static bool IsOpen(void) { return m_Player != NULL; }
};


class cPlaylist;

class cPlayerFactory
{
  public:

    // interact with current player

    static bool IsOpen(void);
    static void Queue (const char *Mrl);

    // launch new media player

    static bool Launch(const char *Mrl, const char *SubFile = NULL) { return Launch(pmNone, Mrl, SubFile); };

    static bool Launch(ePlayMode PlayMode, const char *Mrl, const char *SubFile = NULL, bool BackToMenu = false);
    static bool Launch(ePlayMode PlayMode, cPlaylist *Playlist, bool BackToMenu = false);
};

#endif // __XINELIB_PLAYER_H
