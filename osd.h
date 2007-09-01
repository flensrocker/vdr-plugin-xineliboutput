/*
 * osd.h: Xinelib On Screen Display control
 *
 * See the main source file 'xineliboutput.c' for copyright information and
 * how to reach the author.
 *
 * $Id: osd.h,v 1.4 2007-09-01 08:47:04 phintuka Exp $
 *
 */

#ifndef __XINELIB_OSD_H
#define __XINELIB_OSD_H

#include <vdr/config.h>
#include <vdr/osd.h>
#include <vdr/tools.h>   // cListObject

class cXinelibDevice;

class cXinelibOsd : public cOsd, public cListObject 
{
  private:
    cXinelibOsd();
    cXinelibOsd(cXinelibOsd&);

    cXinelibDevice *m_Device;

  protected:
    static cMutex             m_Lock;
    static cList<cXinelibOsd> m_OsdStack;

    bool   m_IsVisible;
    bool   m_Shown;

    virtual eOsdError CanHandleAreas(const tArea *Areas, int NumAreas);
    virtual eOsdError SetAreas(const tArea *Areas, int NumAreas);
    virtual void Flush(void);

    // Messages from cXinelibOsdProvider
    void Show(void);
    void Hide(void);
    void Refresh(void);
    void Detach(void);

    friend class cXinelibOsdProvider;

  public:
    cXinelibOsd(cXinelibDevice *Device, int x, int y, uint Level = 0);
    virtual ~cXinelibOsd();
};


class cXinelibOsdProvider : public cOsdProvider 
{
  protected:
    cXinelibDevice *m_Device;

  public:
    cXinelibOsdProvider(cXinelibDevice *Device);
    virtual ~cXinelibOsdProvider();

#if VDRVERSNUM >= 10509
    virtual cOsd *CreateOsd(int Left, int Top, uint Level);
#else
    virtual cOsd *CreateOsd(int Left, int Top);
#endif

    static void RefreshOsd(void);
};

#endif //__XINELIB_OSD_H

