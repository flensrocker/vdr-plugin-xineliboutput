/*
 * osd.c: Xinelib On Screen Display control
 *
 * See the main source file 'xineliboutput.c' for copyright information and
 * how to reach the author.
 *
 * $Id: osd.c,v 1.13 2007-10-30 21:07:14 phintuka Exp $
 *
 */

#include <vdr/config.h>
#include <vdr/tools.h>
#include <vdr/thread.h>

#include "logdefs.h"
#include "device.h"
#include "osd.h"
#include "config.h"
#include "xine_osd_command.h"

//#define LIMIT_OSD_REFRESH_RATE

#define LOGOSD(x...)

class cXinelibOsd : public cOsd, public cListObject 
{
  private:
    cXinelibOsd();
    cXinelibOsd(cXinelibOsd&); // no copy

    cXinelibDevice *m_Device;

    void CloseWindows(void);
    void CmdSize(int Width, int Height);
    void CmdRle(int Wnd, int X0, int Y0,
		int W, int H, unsigned char *Data,
		int Colors, unsigned int *Palette);
    void CmdClose(int Wnd);

  protected:
    static cMutex             m_Lock;
    static cList<cXinelibOsd> m_OsdStack;

    bool   m_IsVisible;
    bool   m_Shown;
    uint   m_Layer;

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


void cXinelibOsd::CmdSize(int Width, int Height)
{
  TRACEF("cXinelibOsd::CmdSize");

  if(m_Device) {
    osd_command_t osdcmd;
    memset(&osdcmd,0,sizeof(osdcmd));

    osdcmd.cmd = OSD_Size;
    osdcmd.w = Width;
    osdcmd.h = Height;

    m_Device->OsdCmd((void*)&osdcmd);
  }
}

void cXinelibOsd::CmdClose(int Wnd)
{
  TRACEF("cXinelibOsd::CmdClose");

  if(m_Device) {
    osd_command_t osdcmd;
    memset(&osdcmd,0,sizeof(osdcmd));
    
    osdcmd.cmd = OSD_Close;
    osdcmd.wnd = Wnd;

    m_Device->OsdCmd((void*)&osdcmd);
  }
}

static inline int saturate(int x, int min, int max)
{
  return x < min ? min : (x > max ? max : x);
}

void cXinelibOsd::CmdRle(int Wnd,
			 int X0, int Y0, int W, int H, unsigned char *Data,
			 int Colors, unsigned int *Palette)
{
  TRACEF("cXinelibOsd::CmdRle");

  if(m_Device) {

    xine_rle_elem_t rle, *rle_p=0;

    osd_command_t osdcmd;
    int x, y, num_rle=0, rle_size=0;
    uint8_t *c;
    xine_clut_t clut[256];

    memset(&osdcmd, 0, sizeof(osdcmd));
    memset(&clut, 0, sizeof(clut));
    osdcmd.cmd = OSD_Set_RLE;
    osdcmd.wnd = Wnd;
    osdcmd.x = X0;
    osdcmd.y = Y0;
    osdcmd.w = W;
    osdcmd.h = H;

    /* apply alpha layer correction and convert ARGB -> AYCrCb */

    if (Colors) {
      for(int c=0; c<Colors; c++) {
	int alpha = (Palette[c] & 0xff000000)>>24;
	alpha = alpha + xc.alpha_correction*alpha/100 + xc.alpha_correction_abs;
	int R     = ((Palette[c] & 0x00ff0000) >> 16);
	int G     = ((Palette[c] & 0x0000ff00) >>  8);
	int B     = ((Palette[c] & 0x000000ff));
	int Y     = (( +  66*R + 129*G +  25*B + 0x80) >> 8) +  16;
	int CR    = (( + 112*R -  94*G -  18*B + 0x80) >> 8) + 128;
	int CB    = (( -  38*R -  74*G + 112*B + 0x80) >> 8) + 128;
	clut[c].y     = saturate(    Y, 16, 235);
	clut[c].cb    = saturate(   CB, 16, 240);
	clut[c].cr    = saturate(   CR, 16, 240);
	clut[c].alpha = saturate(alpha,  0, 255);
      }
    }

    osdcmd.colors = Colors;
    osdcmd.palette = clut;

    /* RLE compression */
 
    rle_size = 8128;
    rle_p = (xine_rle_elem_t*)malloc(4*rle_size);
    osdcmd.data = rle_p;

    for( y = 0; y < H; y++ ) {
      rle.len = 0;
      rle.color = 0;
      c = Data + y * W;
      for( x = 0; x < W; x++, c++ ) {
	if( rle.color != *c ) {
	  if( rle.len ) {
	    if( (num_rle + H-y+1) > rle_size ) {
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
    
    TRACE("xinelib_osd.c:CmdRle uncompressed="<< (w*h) <<", compressed=" << (4*num_rle));

    m_Device->OsdCmd((void*)&osdcmd);

    if(osdcmd.data)
      free(osdcmd.data);
  }
}

cXinelibOsd::cXinelibOsd(cXinelibDevice *Device, int x, int y, uint Level)
#if VDRVERSNUM >= 10509
    : cOsd(x, y, Level)
#else
    : cOsd(x, y)
#endif
{
  TRACEF("cXinelibOsd::cXinelibOsd");

  m_Device = Device;
  m_Shown = false;
  m_IsVisible = true;
  m_Layer = Level;
  CmdSize(720, 576);
}

cXinelibOsd::~cXinelibOsd()
{
  TRACEF("cXinelibOsd::~cXinelibOsd");

  cMutexLock ml(&m_Lock);

  CloseWindows();

  m_OsdStack.Del(this,false);

  if(m_OsdStack.First())
    m_OsdStack.First()->Show();
}

eOsdError cXinelibOsd::SetAreas(const tArea *Areas, int NumAreas)
{
  TRACEF("cXinelibOsd::SetAreas");
  cMutexLock ml(&m_Lock);

  LOGOSD("cXinelibOsd::SetAreas, m_Shown = %s", m_Shown ? "true" : "false");

  CloseWindows();

  eOsdError Result = cOsd::SetAreas(Areas, NumAreas);

  if(Left() + Width() > 720 || Top() + Height() > 576) {
    LOGDBG("Detected HD OSD, size > %dx%d, using setup values %dx%d", 
	   Left() + Width(), Top() + Height(), 
	   Setup.OSDWidth, Setup.OSDHeight);
    CmdSize(Setup.OSDWidth, Setup.OSDHeight);
  } else {
    CmdSize(720, 576);
  }

  return Result;
}

eOsdError cXinelibOsd::CanHandleAreas(const tArea *Areas, int NumAreas)
{
  TRACEF("cXinelibOsd::CanHandleAreas");

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
      CmdRle(i,
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

void cXinelibOsd::CloseWindows(void)
{
  TRACEF("cXinelibOsd::CloseWindows");

  if(m_IsVisible) {
    cBitmap *Bitmap;
    m_Shown = false;
    for (int i = 0; (Bitmap = GetBitmap(i)) != NULL; i++) {
      LOGOSD("Close OSD %d.%d", Index(), i);
      CmdClose(i);
    }
  }
}

void cXinelibOsd::Hide(void)
{
  TRACEF("cXinelibOsd::Hide");

  cMutexLock ml(&m_Lock);

  CloseWindows();
  m_IsVisible = false;
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

#if VDRVERSNUM < 10509
  if(cXinelibOsd::m_OsdStack.First())
    LOGMSG("cXinelibOsdProvider::CreateOsd - OSD already open !");
#endif

  cXinelibOsd *m_OsdInstance = new cXinelibOsd(m_Device, Left, Top, Level);

  // sorted insert
  cXinelibOsd *it = cXinelibOsd::m_OsdStack.First(); 
  while(it) {
    if(it->m_Layer >= Level) {
      cXinelibOsd::m_OsdStack.Ins(m_OsdInstance, it);
      break;
    }
    it = cXinelibOsd::m_OsdStack.Next(it);
  }
  if(!it)
    cXinelibOsd::m_OsdStack.Add(m_OsdInstance);

  LOGOSD("New OSD: index %d, layer %d [now %d OSDs]", m_OsdInstance->Index(), Level, cXinelibOsd::m_OsdStack.Count());
  if(xc.osd_mixer == OSD_MIXER_NONE)
    LOGOSD(" OSD mixer off");

  // hide all but top-most OSD  
  it = cXinelibOsd::m_OsdStack.Last();
  while(cXinelibOsd::m_OsdStack.Prev(it)) {
    LOGOSD(" -> hide OSD %d", it->Index());
    it->Hide();
    it = cXinelibOsd::m_OsdStack.Prev(it);
  }
  it->Show();

  return m_OsdInstance;
}

void cXinelibOsdProvider::RefreshOsd(void)
{
  TRACEF("cXinelibOsdProvider::RefreshOsd");

  cMutexLock ml(&cXinelibOsd::m_Lock);

  // bottom --> top (draw lower layer OSDs first)
  cXinelibOsd *it = cXinelibOsd::m_OsdStack.Last();
  while(it) {
    it->Refresh();
    it = cXinelibOsd::m_OsdStack.Prev(it);
  }
}



