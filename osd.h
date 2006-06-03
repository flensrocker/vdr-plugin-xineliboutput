/*
 * osd.h: Xinelib On Screen Display control
 *
 * See the main source file 'xineliboutput.c' for copyright information and
 * how to reach the author.
 *
 * $Id: osd.h,v 1.1 2006-06-03 09:50:54 phintuka Exp $
 *
 */

#ifndef __XINELIB_OSD_H
#define __XINELIB_OSD_H

#include <vdr/config.h>
#include <vdr/osd.h>
#include <vdr/thread.h>  // cMutex
#include <vdr/tools.h>   // cListObject

class cXinelibDevice;

class cXinelibOsd : public cOsd, public cListObject 
{
  private:
    cXinelibOsd();
    cXinelibOsd(cXinelibOsd&);
    cXinelibDevice *m_Device;

  protected:
    cMutex m_Lock;
    bool   m_IsVisible;
    bool   m_Shown;

    virtual eOsdError CanHandleAreas(const tArea *Areas, int NumAreas);
    virtual eOsdError SetAreas(const tArea *Areas, int NumAreas);
    virtual void Flush(void);

    // Messages from cXinelibOsdProvider
    void Show(void);
    void Hide(void);
    void Refresh(void);

    friend class cXinelibOsdProvider;

  public:
    cXinelibOsd(cXinelibDevice *Device, int x, int y);
    virtual ~cXinelibOsd();
};

class cXinelibOsdProvider : public cOsdProvider 
{
  protected:
    cXinelibDevice           *m_Device;
    static cList<cXinelibOsd> m_OsdStack;
    static cMutex             m_Lock;

    // Messages from cXinelibOsd
    static void OsdClosing(cXinelibOsd *Osd);
    static void OsdClosed(cXinelibOsd *Osd);

    friend class cXinelibOsd;

  public:
    cXinelibOsdProvider(cXinelibDevice *Device);
    virtual ~cXinelibOsdProvider();

    virtual cOsd *CreateOsd(int Left, int Top);

    static void RefreshOsd(void);
};

#endif //__XINELIB_OSD_H

