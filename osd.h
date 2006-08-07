/*
 * osd.h: Xinelib On Screen Display control
 *
 * See the main source file 'xineliboutput.c' for copyright information and
 * how to reach the author.
 *
 * $Id: osd.h,v 1.2 2006-08-07 21:09:16 phintuka Exp $
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
    cXinelibOsd(cXinelibDevice *Device, int x, int y);
    virtual ~cXinelibOsd();
};

class cMutex;

class cXinelibOsdProvider : public cOsdProvider 
{
  protected:
    cXinelibDevice *m_Device;

  public:
    cXinelibOsdProvider(cXinelibDevice *Device);
    virtual ~cXinelibOsdProvider();

    virtual cOsd *CreateOsd(int Left, int Top);

    static void RefreshOsd(void);
};

#endif //__XINELIB_OSD_H

