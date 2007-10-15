/*
 * osd.c: Xinelib On Screen Display control
 *
 * See the main source file 'xineliboutput.c' for copyright information and
 * how to reach the author.
 *
 * $Id: osd.c,v 1.9 2007-10-15 00:31:39 phintuka Exp $
 *
 */

#include <vdr/config.h>
#include <vdr/tools.h>   // cListObject

#include "logdefs.h"
#include "device.h"
#include "osd.h"
#include "config.h"

//#define LIMIT_OSD_REFRESH_RATE

#include "xine_osd_command.h"

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

cList<cXinelibOsd> cXinelibOsd::m_OsdStack;
cMutex             cXinelibOsd::m_Lock;

static inline void CmdSize(cXinelibDevice *Device, int wnd, int w=0, int h=0)
{
  TRACEF("xinelib_osd.c:CmdSize");

  if(Device) {
    osd_command_t osdcmd;
    memset(&osdcmd,0,sizeof(osdcmd));

    osdcmd.cmd = OSD_Size;
    osdcmd.wnd = wnd;
    osdcmd.w = w;
    osdcmd.h = h;

    Device->OsdCmd((void*)&osdcmd);
  }
}

static inline void CmdClose(cXinelibDevice *Device, int wnd)
{
  TRACEF("xinelib_osd.c:CmdClose");

  if(Device) {
    osd_command_t osdcmd;
    memset(&osdcmd,0,sizeof(osdcmd));
    
    osdcmd.cmd = OSD_Close;
    osdcmd.wnd = wnd;

    Device->OsdCmd((void*)&osdcmd);
  }
}

static inline int saturate(int x, int min, int max)
{
  return x < min ? min : (x > max ? max : x);
}

static inline void RleCmd(cXinelibDevice *Device, int wnd,
                int x0, int y0, int w, int h, unsigned char *data,
                int colors, unsigned int *palette)
{
  TRACEF("xinelib_osd.c:RleCmd");

  if(Device) {

    xine_rle_elem_t rle, *rle_p=0;

    osd_command_t osdcmd;
    int x, y, num_rle=0, rle_size=0;
    uint8_t *c;
    xine_clut_t clut[256];

    memset(&osdcmd, 0, sizeof(osdcmd));
    memset(&clut, 0, sizeof(clut));
    osdcmd.cmd = OSD_Set_RLE;
    osdcmd.wnd = wnd;
    osdcmd.x = x0;
    osdcmd.y = y0;
    osdcmd.w = w;
    osdcmd.h = h;

    /* apply alpha layer correction and convert ARGB -> AYCrCb */

    if (colors) {
      for(int c=0; c<colors; c++) {
	int alpha = (palette[c] & 0xff000000)>>24;
	alpha = alpha + xc.alpha_correction*alpha/100 + xc.alpha_correction_abs;
	int R     = ((palette[c] & 0x00ff0000) >> 16);
	int G     = ((palette[c] & 0x0000ff00) >>  8);
	int B     = ((palette[c] & 0x000000ff));
	int Y     = (( +  66*R + 129*G +  25*B + 0x80) >> 8) +  16;
	int CR    = (( + 112*R -  94*G -  18*B + 0x80) >> 8) + 128;
	int CB    = (( -  38*R -  74*G + 112*B + 0x80) >> 8) + 128;
	clut[c].y     = saturate(    Y, 16, 235);
	clut[c].cb    = saturate(   CB, 16, 240);
	clut[c].cr    = saturate(   CR, 16, 240);
	clut[c].alpha = saturate(alpha,  0, 255);
      }
    }

    osdcmd.colors = colors;
    osdcmd.palette = clut;

    /* RLE compression */
 
    rle_size = 8128;
    rle_p = (xine_rle_elem_t*)malloc(4*rle_size);
    osdcmd.data = rle_p;

    for( y = 0; y < h; y++ ) {
      rle.len = 0;
      rle.color = 0;
      c = data + y * w;
      for( x = 0; x < w; x++, c++ ) {
	if( rle.color != *c ) {
	  if( rle.len ) {
	    if( (num_rle + h-y+1) > rle_size ) {
	      rle_size *= 2;
	      rle_p = (xine_rle_elem_t*)realloc( osdcmd.data, 4*rle_size);
	      osdcmd.data = rle_p;
	      rle_p += num_rle;
	    }
	    *rle_p++ = rle;
	    num_rle++;
	  }
	  rle.color = *c;
	  rle.len = 1;
	} else {
	  rle.len++;
	}
      }
      *rle_p++ = rle;
      num_rle++;
    }
    osdcmd.datalen = 4 * num_rle;
    osdcmd.num_rle = num_rle;
    
    TRACE("xinelib_osd.c:RleCmd uncompressed="<< (w*h) <<", compressed=" << (4*num_rle));

    Device->OsdCmd((void*)&osdcmd);

    if(osdcmd.data)
      free(osdcmd.data);
  }
}

cXinelibOsd::cXinelibOsd(cXinelibDevice *Device, int x, int y, uint Level)
#if VDRVERSNUM >= 10509
    : cOsd(x, y, Level), m_IsVisible(true)
#else
    : cOsd(x, y), m_IsVisible(true)
#endif
{
  TRACEF("cXinelibOsd::cXinelibOsd");

  m_Device = Device;
  m_Shown = false;
  CmdSize(m_Device, 0, 720, 576);
}

cXinelibOsd::~cXinelibOsd()
{
  TRACEF("cXinelibOsd::~cXinelibOsd");

  cMutexLock ml(&m_Lock);

  if(m_IsVisible)
    Hide();

  m_OsdStack.Del(this,false);

  if(m_OsdStack.First())
    m_OsdStack.First()->Show();
}

eOsdError cXinelibOsd::SetAreas(const tArea *Areas, int NumAreas)
{
  TRACEF("cXinelibOsd::SetAreas");
  cMutexLock ml(&m_Lock);

  eOsdError Result = cOsd::SetAreas(Areas, NumAreas);
  if(Result == oeOk) 
    m_Shown = false;

  if(Left() + Width() > 720 || Top() + Height() > 576) {
    LOGDBG("Detected HD OSD, size > %dx%d, using setup values %dx%d", 
	   Left() + Width(), Top() + Height(), 
	   Setup.OSDWidth, Setup.OSDHeight);
    CmdSize(m_Device, 0, Setup.OSDWidth, Setup.OSDHeight);
  } else {
    CmdSize(m_Device, 0, 720, 576);
  }

  return Result;
}

eOsdError cXinelibOsd::CanHandleAreas(const tArea *Areas, int NumAreas)
{
  TRACEF("cXinelibOsd::CanHandleAreas");

  m_Shown = false;
  eOsdError Result = cOsd::CanHandleAreas(Areas, NumAreas);
  if (Result == oeOk) {
    if (NumAreas > MAX_OSD_OBJECT)
      return oeTooManyAreas;
    for (int i = 0; i < NumAreas; i++) {
      if (Areas[i].bpp != 1 && Areas[i].bpp != 2 && 
	  Areas[i].bpp != 4 && Areas[i].bpp != 8)
        return oeBppNotSupported;
    }
  }
  return Result;
}

void cXinelibOsd::Flush(void)
{
  TRACEF("cXinelibOsd::Flush");

  cMutexLock ml(&m_Lock);

  cBitmap *Bitmap;

  if(!m_IsVisible) 
    return;

  int SendDone = 0;
  for (int i = 0; (Bitmap = GetBitmap(i)) != NULL; i++) {
    int x1 = 0, y1 = 0, x2 = Bitmap->Width()-1, y2 = Bitmap->Height()-1;
    if (!m_Shown || Bitmap->Dirty(x1, y1, x2, y2)) {

      /* XXX what if only palette has been changed ? */
      int NumColors;
      const tColor *Colors = Bitmap->Colors(NumColors);
      RleCmd(m_Device, i,
             Left() + Bitmap->X0(), Top() + Bitmap->Y0(),
             Bitmap->Width(), Bitmap->Height(),
             (unsigned char *)Bitmap->Data(0,0),
             NumColors, (unsigned int *)Colors);
      SendDone++;
    }
    Bitmap->Clean();
  }

#ifdef LIMIT_OSD_REFRESH_RATE
  if(SendDone) {
    static int64_t last_refresh = 0LL;
    int64_t now = cTimeMs::Now();
    if(now - last_refresh < 100) {
      /* too fast refresh rate, delay ... */
      cCondWait::SleepMs(40); /* Can't update faster anyway ... */
# if 0
      LOGDBG("cXinelibOsd::Flush: OSD refreshing too fast ! (>10Hz) -> Sleeping 50ms");
# endif
    }
    last_refresh = now;
  }
#endif

#ifdef YAEPG_PATCH
  // yaepg
  if(!m_Shown && vidWin.bpp != 0) {
    LOGDBG("yaepg vidWin %d %d %d %d\n", 
	   vidWin.x1, vidWin.y1, vidWin.x2, vidWin.y2);
    fflush(stdout);
  }
#endif

  m_Shown = true;
}

void cXinelibOsd::Refresh(void)
{
  TRACEF("cXinelibOsd::Refresh");

  cMutexLock ml(&m_Lock);

  m_Shown = false;
  Flush();
}

void cXinelibOsd::Show(void)
{
  TRACEF("cXinelibOsd::Show");

  cMutexLock ml(&m_Lock);

  m_IsVisible = true;
  Refresh();
}

void cXinelibOsd::Hide(void)
{
  TRACEF("cXinelibOsd::Hide");

  cMutexLock ml(&m_Lock);

  if(m_IsVisible) {
    cBitmap *Bitmap;
    m_IsVisible = false;
    for (int i = 0; (Bitmap = GetBitmap(i)) != NULL; i++)
      CmdClose(m_Device, i);
  }
}

void cXinelibOsd::Detach(void)
{
  TRACEF("cXinelibOsd::Detach");

  cMutexLock ml(&m_Lock);

  Hide();
  m_Device = NULL;
}

//
// cXinelibOsdProvider
//

cXinelibOsdProvider::cXinelibOsdProvider(cXinelibDevice *Device)
{
  m_Device = Device;
}

cXinelibOsdProvider::~cXinelibOsdProvider()
{
  LOGMSG("cXinelibOsdProvider: shutting down !");

  cMutexLock ml(&cXinelibOsd::m_Lock);

  m_Device = NULL;

  if(cXinelibOsd::m_OsdStack.First()) {
    LOGMSG("cXinelibOsdProvider: OSD open while OSD provider shutting down !");

    // Detach all OSD instances from device
    cXinelibOsd *osd;
    while(NULL != (osd = cXinelibOsd::m_OsdStack.First())) {
      osd->Detach();
      cXinelibOsd::m_OsdStack.Del(osd, false);
    }
  }
}

cOsd *cXinelibOsdProvider::CreateOsd(int Left, int Top, uint Level)
{
  TRACEF("cXinelibOsdProvider::CreateOsd");

  cMutexLock ml(&cXinelibOsd::m_Lock);

  if(cXinelibOsd::m_OsdStack.First())
    LOGMSG("cXinelibOsdProvider::CreateOsd - OSD already open !");

  cXinelibOsd *m_OsdInstance = new cXinelibOsd(m_Device, Left, Top, Level);

  if(cXinelibOsd::m_OsdStack.First())
    cXinelibOsd::m_OsdStack.First()->Hide();

  cXinelibOsd::m_OsdStack.Ins(m_OsdInstance);

  return m_OsdInstance;
}

void cXinelibOsdProvider::RefreshOsd(void)
{
  TRACEF("cXinelibOsdProvider::RefreshOsd");

  cMutexLock ml(&cXinelibOsd::m_Lock);

  if(cXinelibOsd::m_OsdStack.First())
    cXinelibOsd::m_OsdStack.First()->Refresh();
}



