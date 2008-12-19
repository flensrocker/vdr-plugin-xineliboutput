/*
 * osd.c: Xinelib On Screen Display control
 *
 * See the main source file 'xineliboutput.c' for copyright information and
 * how to reach the author.
 *
 * $Id: osd.c,v 1.30 2008-12-19 15:17:13 phintuka Exp $
 *
 */

#include <vdr/config.h>
#include <vdr/tools.h>
#include <vdr/thread.h>

#include "logdefs.h"
#include "device.h"
#include "config.h"
#include "xine_osd_command.h"
#include "tools/rle.h"

#include "osd.h"

//#define LIMIT_OSD_REFRESH_RATE

#define LOGOSD(x...)

//
// tools
//

static inline int saturate(int x, int min, int max)
{
  return x < min ? min : (x > max ? max : x);
}

static inline void prepare_palette(xine_clut_t *clut, const unsigned int *palette, int colors, bool top, bool rgb)
{
  if (colors) {
    int c;

    // Apply alpha layer correction and convert ARGB -> AYCrCb

    for(c=0; c<colors; c++) {
      int A = (palette[c] & 0xff000000) >> 24;
      int R = (palette[c] & 0x00ff0000) >> 16;
      int G = (palette[c] & 0x0000ff00) >>  8;
      int B = (palette[c] & 0x000000ff);
      A = A + xc.alpha_correction*A/100 + xc.alpha_correction_abs;
      if(rgb) {
        clut[c].r     = R;
        clut[c].g     = G;
        clut[c].b     = B;
	clut[c].alpha = saturate( A,  0, 255);
      } else {
        int Y         = (( +  66*R + 129*G +  25*B + 0x80) >> 8) +  16;
        int CR        = (( + 112*R -  94*G -  18*B + 0x80) >> 8) + 128;
        int CB        = (( -  38*R -  74*G + 112*B + 0x80) >> 8) + 128;
        clut[c].y     = saturate( Y, 16, 235);
        clut[c].cb    = saturate(CB, 16, 240);
        clut[c].cr    = saturate(CR, 16, 240);
        clut[c].alpha = saturate( A,  0, 255);
      }
    }

    // Apply OSD mixer settings

    if(!top) {
      if(xc.osd_mixer & OSD_MIXER_ALPHA)
	for(c=0; c<colors; c++)
	  clut[c].alpha = (clut[c].alpha >> 1) | 0x80;
      if(xc.osd_mixer & OSD_MIXER_GRAY)
	for(c=0; c<colors; c++) {
	  if(rgb)
	    clut[c].r = clut[c].g = clut[c].b = (clut[c].r + clut[c].g + clut[c].b)/3;
	  else
	    clut[c].cb = clut[c].cr = 0x80;
	}
    }
  }
}

//
// cXinelibOsd
//

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
		int Colors, unsigned int *Palette, 
		osd_rect_t *DirtyArea);
    void CmdPalette(int Wnd, int Colors, unsigned int *Palette);
    void CmdMove(int Wnd, int Width, int Height);
    void CmdClose(int Wnd);
    void CmdFlush(void);

  protected:
    static cMutex             m_Lock;
    static cList<cXinelibOsd> m_OsdStack;

    bool   m_IsVisible;
    bool   m_Refresh;
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

  if (m_Device) {
    osd_command_t osdcmd = {0};

    osdcmd.cmd = OSD_Size;
    osdcmd.w   = Width;
    osdcmd.h   = Height;

    m_Device->OsdCmd((void*)&osdcmd);
  }
}

void cXinelibOsd::CmdMove(int Wnd, int NewX, int NewY)
{
  TRACEF("cXinelibOsd::CmdMove");

  if (m_Device) {
    osd_command_t osdcmd = {0};

    osdcmd.cmd = OSD_Move;
    osdcmd.wnd = Wnd;
    osdcmd.x   = NewX;
    osdcmd.y   = NewY;

    m_Device->OsdCmd((void*)&osdcmd);
  }
}

void cXinelibOsd::CmdPalette(int Wnd, int Colors, unsigned int *Palette)
{
  TRACEF("cXinelibOsd::CmdPalette");

  if (m_Device) {
    xine_clut_t   clut[Colors];
    osd_command_t osdcmd = {0};

    osdcmd.cmd     = OSD_SetPalette;
    osdcmd.wnd     = Wnd;
    osdcmd.palette = clut;
    osdcmd.colors  = Colors;

    prepare_palette(&clut[0], Palette, Colors, /*Top*/(Prev() == NULL), true);

    m_Device->OsdCmd((void*)&osdcmd);
  }
}

void cXinelibOsd::CmdClose(int Wnd)
{
  TRACEF("cXinelibOsd::CmdClose");

  if (m_Device) {
    osd_command_t osdcmd = {0};

    osdcmd.cmd = OSD_Close;
    osdcmd.wnd = Wnd;

    if (m_Refresh)
      osdcmd.flags |= OSDFLAG_REFRESH;
    if (Prev() == NULL)
      osdcmd.flags |= OSDFLAG_TOP_LAYER;

    m_Device->OsdCmd((void*)&osdcmd);
  }
}

void cXinelibOsd::CmdFlush(void)
{
  TRACEF("cXinelibOsd::CmdFlush");

  if (m_Device) {
    osd_command_t osdcmd = {0};

    osdcmd.cmd = OSD_Flush;

    m_Device->OsdCmd((void*)&osdcmd);
  }
}

void cXinelibOsd::CmdRle(int Wnd, int X0, int Y0, 
			 int W, int H, unsigned char *Data,
			 int Colors, unsigned int *Palette, 
			 osd_rect_t *DirtyArea)
{
  TRACEF("cXinelibOsd::CmdRle");

  if (m_Device) {

    xine_clut_t   clut[Colors];
    osd_command_t osdcmd = {0};

    osdcmd.cmd   = OSD_Set_RLE;
    osdcmd.wnd   = Wnd;
    osdcmd.layer = saturate(m_Layer, 0, 0xffff);
    osdcmd.x   = X0;
    osdcmd.y   = Y0;
    osdcmd.w   = W;
    osdcmd.h   = H;
    osdcmd.colors  = Colors;
    osdcmd.palette = clut;
    osdcmd.scaling = xc.osd_scaling;

    if (DirtyArea)
      memcpy(&osdcmd.dirty_area, DirtyArea, sizeof(osd_rect_t));
    if (m_Refresh)
      osdcmd.flags |= OSDFLAG_REFRESH;
    if (xc.osd_blending == OSD_BLENDING_HARDWARE)
      osdcmd.flags |= OSDFLAG_UNSCALED;
    if (xc.osd_blending_lowresvideo == OSD_BLENDING_HARDWARE)
      osdcmd.flags |= OSDFLAG_UNSCALED_LOWRES;
    if (Prev() == NULL)
      osdcmd.flags |= OSDFLAG_TOP_LAYER;

    prepare_palette(&clut[0], Palette, Colors, /*Top*/(Prev() == NULL), true);

    osdcmd.num_rle = rle_compress(&osdcmd.data, Data, W, H);
    osdcmd.datalen = 4 * osdcmd.num_rle;

    m_Device->OsdCmd((void*)&osdcmd);

    free(osdcmd.data);
  }
}

cXinelibOsd::cXinelibOsd(cXinelibDevice *Device, int x, int y, uint Level)
    : cOsd(x, y, Level)
{
  TRACEF("cXinelibOsd::cXinelibOsd");

  m_Device = Device;
  m_Refresh = false;
  m_IsVisible = true;
  m_Layer = Level;
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

  LOGOSD("cXinelibOsd::SetAreas");

  CloseWindows();

  eOsdError Result = cOsd::SetAreas(Areas, NumAreas);

  if(Left() + Width() > 720 || Top() + Height() > 576) {
    LOGDBG("Detected HD OSD, size > %dx%d, using setup values %dx%d", 
           2*Left() + Width(), 2*Top() + Height(),
           Setup.OSDWidth + (2*Setup.OSDLeft), Setup.OSDHeight + (2*Setup.OSDTop));
    CmdSize(Setup.OSDWidth + (2*Setup.OSDLeft), Setup.OSDHeight + (2*Setup.OSDTop));
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
    if (m_Refresh || Bitmap->Dirty(x1, y1, x2, y2)) {

      /* XXX what if only palette has been changed ? */
      int NumColors;
      const tColor *Colors = Bitmap->Colors(NumColors);
      if (Colors) {
	osd_rect_t DirtyArea = {x1:x1, y1:y1, x2:x2, y2:y2};
	CmdRle(i,
	       Left() + Bitmap->X0(), Top() + Bitmap->Y0(),
	       Bitmap->Width(), Bitmap->Height(),
	       (unsigned char *)Bitmap->Data(0,0),
	       NumColors, (unsigned int *)Colors,
	       &DirtyArea);
	SendDone++;
      }
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
}

void cXinelibOsd::Refresh(void)
{
  TRACEF("cXinelibOsd::Refresh");

  cMutexLock ml(&m_Lock);

  m_Refresh = true;
  CloseWindows();
  Flush();
  m_Refresh = false;
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
    for (int i = 0; (Bitmap = GetBitmap(i)) != NULL; i++) {
      LOGOSD("Close OSD %d.%d", Index(), i);
      CmdClose(i);
    }
  }

  if (!m_Refresh)
    CmdFlush();
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



