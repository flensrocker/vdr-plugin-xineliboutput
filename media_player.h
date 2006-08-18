/*
 * media_player.h: Media and image players
 *
 * See the main source file 'xineliboutput.c' for copyright information and
 * how to reach the author.
 *
 * $Id: media_player.h,v 1.3 2006-08-18 04:24:49 phintuka Exp $
 *
 */

#ifndef __XINELIB_PLAYER_H
#define __XINELIB_PLAYER_H

#include <vdr/player.h>

// --- Media player ---------------------------------------------------------

class cXinelibPlayer;
class cSkinDisplayReplay;

class cXinelibPlayerControl : public cControl 
{
  private:
    static cXinelibPlayer *m_Player;
    static cMutex m_Lock;

    static cXinelibPlayer *OpenPlayer(const char *file);

 protected:
    cSkinDisplayReplay *m_DisplayReplay;

    int   m_Speed;
    bool  m_ShowModeOnly;

    static int m_SubtitlePos;

  public:
    cXinelibPlayerControl(const char *file);
    virtual ~cXinelibPlayerControl();

    virtual void Show(void);
    virtual void Hide(void);
    virtual eOSState ProcessKey(eKeys Key);

    static void Close(void);
    static bool IsOpen(void) {return m_Player != NULL;};
};


// --- DVD player -----------------------------------------------------------

class cDvdMenu;
class cXinelibDvdPlayerControl : public cXinelibPlayerControl
{
  private:
    cDvdMenu *Menu;

  public:
    cXinelibDvdPlayerControl(const char *file) : 
      cXinelibPlayerControl(file), Menu(NULL)
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

    static cXinelibImagePlayer *OpenPlayer(const char *file);

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

