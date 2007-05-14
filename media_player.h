/*
 * media_player.h: Media and image players
 *
 * See the main source file 'xineliboutput.c' for copyright information and
 * how to reach the author.
 *
 * $Id: media_player.h,v 1.10 2007-05-14 18:04:33 phintuka Exp $
 *
 */

#ifndef __XINELIB_PLAYER_H
#define __XINELIB_PLAYER_H

#include <vdr/player.h>

#include "config.h"

// --- Media player ---------------------------------------------------------

class cXinelibPlayer;
class cSkinDisplayReplay;
class cPlaylistMenu;

class cXinelibPlayerControl : public cControl 
{
  private:
    static cMutex m_Lock;

    static cXinelibPlayer *OpenPlayer(const char *File, bool Queue = false);

 protected:
    static cXinelibPlayer *m_Player;

    cSkinDisplayReplay *m_DisplayReplay;
    cPlaylistMenu *m_PlaylistMenu;

    eMainMenuMode m_Mode;
    bool   m_ShowModeOnly;
    bool   m_RandomPlay;
    time_t m_AutoShowStart;
    int    m_CurrentPos;
    int    m_CurrentLen;
    bool   m_BlinkState;
    static int m_SubtitlePos;

    void MsgReplaying(const char *Title, const char *File);

  public:
    cXinelibPlayerControl(eMainMenuMode Mode, const char *File);
    virtual ~cXinelibPlayerControl();

    virtual void Show(void);
    virtual void Hide(void);
    virtual eOSState ProcessKey(eKeys Key);

    virtual cOsdObject *GetInfo(void);

    static void Close(void);
    static bool IsOpen(void) { return m_Player != NULL; };
    static void Queue(const char *File);
};


// --- DVD player -----------------------------------------------------------

class cDvdMenu;
class cXinelibDvdPlayerControl : public cXinelibPlayerControl
{
  private:
    cDvdMenu *Menu;

  public:
    cXinelibDvdPlayerControl(const char *File) : 
      cXinelibPlayerControl(ShowFiles, File), Menu(NULL)
      {}
    virtual ~cXinelibDvdPlayerControl();
 
    virtual void Show(void);
    virtual void Hide(void);
    virtual eOSState ProcessKey(eKeys Key);
};

// --- Image player ---------------------------------------------------------

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

    static void Close(void);
    static bool IsOpen(void) {return m_Player != NULL;};
};

#endif // __XINELIB_PLAYER_H

