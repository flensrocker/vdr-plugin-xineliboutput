/*
 * setup_menu.c: Setup Menu
 *
 * See the main source file 'xineliboutput.c' for copyright information and
 * how to reach the author.
 *
 * $Id: setup_menu.c,v 1.1 2006-06-03 10:01:17 phintuka Exp $
 *
 */

#include <vdr/config.h>
#include <vdr/i18n.h>
#include <vdr/plugin.h>

#include "setup_menu.h"
#include "device.h"
#include "menuitems.h"
#include "config.h"

namespace XinelibOutputSetupMenu {

//#define INTEGER_CONFIG_VIDEO_CONTROLS
//#define LINEAR_VIDEO_CONTROLS

//--- Setup Menu -------------------------------------------------------------

const char *ModeLineChars = 
  " 0123456789+-hvsync.";
const char *DriverNameChars = 
  " abcdefghijklmnopqrstuvwxyz0123456789-.,#~:;";
const char *OptionsChars = 
  "=.,abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";

const char *controls[] =
  { "Off",
    "[|---------------]","[|---------------]",
    "[-|--------------]","[-|--------------]",
    "[--|-------------]","[--|-------------]",
    "[---|------------]","[---|------------]",
    "[----|-----------]","[----|-----------]",
    "[-----|----------]","[-----|----------]",
    "[------|---------]","[------|---------]",
    "[-------|--------]","[-------|--------]",
    "[--------|-------]","[--------|-------]",
    "[---------|------]","[---------|------]",
    "[----------|-----]","[----------|-----]",
    "[-----------|----]","[-----------|----]",
    "[------------|---]","[------------|---]",
    "[-------------|--]","[-------------|--]",
    "[--------------|-]","[--------------|-]",
    "[---------------|]","[---------------|]",
    NULL
  };

#ifdef LINEAR_VIDEO_CONTROLS
#  define CONTROL_TO_INDEX(val) ((val)>=0 ? ((val)>>11)+1 : 0)
#  define INDEX_TO_CONTROL(ind) ((ind)==0 ? -1 : ((ind)-1)<<11)
#else
const int ind2ctrl_tbl[33] = {
      -1,      0, 0x0001, 0x0002, 0x0003, 0x0004, 0x0007, 0x000a, 
  0x000f, 0x0014, 0x001f,     42, 0x003f,     80, 0x007f,    170, 
  0x00ff,    336, 0x01ff,    682, 0x03ff,   1630, 0x07ff,   2730,
  0x0fff,   5726, 0x1fff,  10858, 0x3fff,  22110, 0x7fff,  43224,
  0xffff  };
static int CONTROL_TO_INDEX(int val) 
{
  for(int i=0; i<33;i++)
    if(val<=ind2ctrl_tbl[i])
      return i;
  return 32;
}
static int INDEX_TO_CONTROL(int ind) 
{
  if(ind<0) ind=0;
  if(ind>32) ind=32;
  return ind2ctrl_tbl[ind];
}
#endif

static cOsdItem *NewTitle(const char *s)
{
  char str[128];
  cOsdItem *tmp;
  sprintf(str,"----- %s -----", tr(s));
  tmp = new cOsdItem(str);
  tmp->SetSelectable(false);
  return tmp;
}

//--- cMenuSetupAudio --------------------------------------------------------

class cMenuSetupAudio : public cMenuSetupPage 
{
  private:
    config_t newconfig;
    int audio_driver;
    int visualization;

    cOsdItem *audio_driver_item;
    cOsdItem *audio_ctrl_delay;
    cOsdItem *audio_ctrl_compression;
    cOsdItem *audio_ctrl_upmix;
    cOsdItem *audio_ctrl_surround;
    cOsdItem *audio_ctrl_headphone;
    cMenuEditItem *audio_port_item;
  
  protected:
    virtual void Store(void);
    void Set(void);
  
  public:
    cMenuSetupAudio(void);
    ~cMenuSetupAudio(void);

    virtual eOSState ProcessKey(eKeys Key);
};

cMenuSetupAudio::cMenuSetupAudio(void) 
{
  memcpy(&newconfig, &xc, sizeof(config_t));
  audio_driver  = strstra(xc.audio_driver, 
			  xc.s_audioDrivers, 
			  0);
  visualization = strstra(xc.audio_visualization, 
			  xc.s_audioVisualizations, 
			  0);
  Set();
}

cMenuSetupAudio::~cMenuSetupAudio(void) 
{
  cXinelibDevice::Instance().ConfigurePostprocessing(
        xc.deinterlace_method, xc.audio_delay, xc.audio_compression, 
	xc.audio_equalizer, xc.audio_surround);
#ifdef ENABLE_TEST_POSTPLUGINS
  cXinelibDevice::Instance().ConfigurePostprocessing(
        "upmix", xc.audio_upmix ? true : false, NULL);
  cXinelibDevice::Instance().ConfigurePostprocessing(
        "headphone", xc.headphone ? true : false, NULL);
#endif
}

void cMenuSetupAudio::Set(void)
{
  SetPlugin(cPluginManager::GetPlugin(PLUGIN_NAME_I18N));
  int current = Current();
  Clear();

  Add(NewTitle("Audio"));
  Add(audio_driver_item = 
      new cMenuEditStraI18nItem(tr("Driver"), &audio_driver, 
			    AUDIO_DRIVER_count, xc.s_audioDriverNames));
  if(audio_driver != AUDIO_DRIVER_AUTO && audio_driver != AUDIO_DRIVER_NONE)
    Add(audio_port_item = 
	new cMenuEditStrItem(tr("Port"), newconfig.audio_port, 31, 
			     DriverNameChars));
  else
    audio_port_item = NULL;
  Add(audio_ctrl_delay = 
      new cMenuEditTypedIntItem(tr("Delay"), tr("ms"), &newconfig.audio_delay,
				-3000, 3000, tr("Off")));
  Add(audio_ctrl_compression = 
      new cMenuEditTypedIntItem(tr("Audio Compression"), "%", 
				&newconfig.audio_compression, 
				100, 500, tr("Off")));
  Add(audio_ctrl_upmix =
      new cMenuEditBoolItem(tr("Upmix stereo to 5.1"), 
			    &newconfig.audio_upmix));
  Add(audio_ctrl_surround =
      new cMenuEditBoolItem(tr("Downmix AC3 to surround"), 
			    &newconfig.audio_upmix));
#ifdef ENABLE_TEST_POSTPLUGINS
  Add(audio_ctrl_headphone =
      new cMenuEditBoolItem(tr("Mix to headphones"), 
			    &newconfig.headphone));
#else
  audio_ctrl_headphone = NULL;
#endif
  Add(new cMenuEditStraI18nItem(tr("Visualization"), &visualization, 
			    AUDIO_VIS_count, 
			    xc.s_audioVisualizationNames));

  if(current<1) current=1; /* first item is not selectable */
  SetCurrent(Get(current));
  Display();
}

eOSState cMenuSetupAudio::ProcessKey(eKeys Key)
{
  cOsdItem *item = Get(Current());
  int       val  = audio_driver;

  eOSState state = cMenuSetupPage::ProcessKey(Key);

  if(Key!=kLeft && Key!=kRight)
    return state;

  if(item == audio_driver_item) {
    if(val != audio_driver) {
      if(audio_driver == AUDIO_DRIVER_ALSA) {
        strcpy(newconfig.audio_port, "default");
        Set();
      } else if(audio_driver == AUDIO_DRIVER_OSS) {
        strcpy(newconfig.audio_port, "/dev/dsp0");
        Set();
      } else {
        strcpy(newconfig.audio_port, "");
        Set();
      }
    }
    else if((audio_driver != AUDIO_DRIVER_AUTO && 
	     audio_driver != AUDIO_DRIVER_NONE) && 
	    !audio_port_item)
      Set();
    else if((audio_driver == AUDIO_DRIVER_AUTO || 
	     audio_driver == AUDIO_DRIVER_NONE) && 
	    audio_port_item)
      Set();
  }
  else if(item == audio_ctrl_delay || item == audio_ctrl_compression || 
	  item == audio_ctrl_surround) {
    cXinelibDevice::Instance().ConfigurePostprocessing(
	  xc.deinterlace_method, newconfig.audio_delay, 
	  newconfig.audio_compression, newconfig.audio_equalizer,
	  newconfig.audio_surround);
  }
  else if(item == audio_ctrl_upmix) {
    cXinelibDevice::Instance().ConfigurePostprocessing(
	  "upmix", newconfig.audio_upmix ? true : false, NULL);
  }
#ifdef ENABLE_TEST_POSTPLUGINS
  else if(item == audio_ctrl_headphone) {
    cXinelibDevice::Instance().ConfigurePostprocessing(
	  "headphone", newconfig.headphone ? true : false, NULL);
  }
#endif

  return state;
}


void cMenuSetupAudio::Store(void)
{
  memcpy(&xc, &newconfig, sizeof(config_t));
  strcpy(xc.audio_driver, xc.s_audioDrivers[audio_driver]);
  strcpy(xc.audio_visualization, xc.s_audioVisualizations[visualization]);

  SetupStore("Audio.Driver", xc.audio_driver);
  SetupStore("Audio.Port",   xc.audio_port);
  SetupStore("Audio.Delay",  xc.audio_delay);
  SetupStore("Audio.Compression",  xc.audio_compression);
  SetupStore("Audio.Surround",     xc.audio_surround);
  SetupStore("Audio.Upmix",        xc.audio_upmix);
  SetupStore("Audio.Headphone",    xc.headphone);
  SetupStore("Audio.Visualization",xc.audio_visualization);
}

//--- cMenuSetupAudioEq ------------------------------------------------------

class cMenuSetupAudioEq : public cMenuSetupPage 
{
  private:
    config_t newconfig;
  
  protected:
    virtual void Store(void);
    void Set(void);
  
  public:
    cMenuSetupAudioEq(void);
    ~cMenuSetupAudioEq(void);

    virtual eOSState ProcessKey(eKeys Key);
};

cMenuSetupAudioEq::cMenuSetupAudioEq(void) 
{
  memcpy(&newconfig, &xc, sizeof(config_t));
  Set();
}

cMenuSetupAudioEq::~cMenuSetupAudioEq(void) 
{
  cXinelibDevice::Instance().ConfigurePostprocessing(
        xc.deinterlace_method, xc.audio_delay, xc.audio_compression, 
	xc.audio_equalizer, xc.audio_surround);
}

void cMenuSetupAudioEq::Set(void)
{
  SetPlugin(cPluginManager::GetPlugin(PLUGIN_NAME_I18N));
  int current = Current();
  Clear();

  Add(NewTitle("Audio Equalizer"));
  for(int i=0; i<AUDIO_EQ_count; i++)
    Add(new cMenuEditTypedIntItem(config_t::s_audioEqNames[i], "%", 
				  &newconfig.audio_equalizer[i],
				  -100, 100, tr("Off")));

  if(current<1) current=1; /* first item is not selectable */
  SetCurrent(Get(current));
  Display();
}

eOSState cMenuSetupAudioEq::ProcessKey(eKeys Key)
{
  eOSState state = cMenuSetupPage::ProcessKey(Key);

  if(Key == kLeft || Key == kRight) {
    cXinelibDevice::Instance().ConfigurePostprocessing(
	xc.deinterlace_method, xc.audio_delay, xc.audio_compression, 
	newconfig.audio_equalizer, xc.audio_surround);
  }

  return state;
}

void cMenuSetupAudioEq::Store(void)
{
  memcpy(&xc, &newconfig, sizeof(config_t));

  char tmp[255];
  sprintf(tmp,"%d %d %d %d %d %d %d %d %d %d",
	  xc.audio_equalizer[0], xc.audio_equalizer[1],
	  xc.audio_equalizer[2], xc.audio_equalizer[3],
	  xc.audio_equalizer[4], xc.audio_equalizer[5],
	  xc.audio_equalizer[6], xc.audio_equalizer[7],
	  xc.audio_equalizer[8], xc.audio_equalizer[9]);
  SetupStore("Audio.Equalizer", tmp);
}

//--- cMenuSetupVideo --------------------------------------------------------

class cMenuSetupVideo : public cMenuSetupPage 
{
  private:
    config_t newconfig;

    cOsdItem *ctrl_hue;
    cOsdItem *ctrl_saturation;
    cOsdItem *ctrl_contrast;
    cOsdItem *ctrl_brightness;
  
  protected:
    virtual void Store(void);
    void Set(void);
  
  public:
    cMenuSetupVideo(void);
    ~cMenuSetupVideo(void);

    virtual eOSState ProcessKey(eKeys Key);
};

cMenuSetupVideo::cMenuSetupVideo(void)
{
  memcpy(&newconfig, &xc, sizeof(config_t));

  newconfig.hue        = CONTROL_TO_INDEX(newconfig.hue);
  newconfig.saturation = CONTROL_TO_INDEX(newconfig.saturation);
  newconfig.contrast   = CONTROL_TO_INDEX(newconfig.contrast);
  newconfig.brightness = CONTROL_TO_INDEX(newconfig.brightness);

  Set();
}

cMenuSetupVideo::~cMenuSetupVideo(void)
{
  cXinelibDevice::Instance().ConfigureVideo(xc.hue, xc.saturation, 
					    xc.brightness, xc.contrast);
}

void cMenuSetupVideo::Set(void)
{
  SetPlugin(cPluginManager::GetPlugin(PLUGIN_NAME_I18N));
  //int current = Current();
  Clear();

  Add(NewTitle("Video"));

#ifdef INTEGER_CONFIG_VIDEO_CONTROLS
  Add(new cMenuEditIntItem(tr("HUE"), &newconfig.hue, -1, 0xffff));
  Add(new cMenuEditIntItem(tr("Saturation"), &newconfig.saturation,-1,0xffff));
  Add(new cMenuEditIntItem(tr("Contrast"),   &newconfig.contrast, -1, 0xffff));
  Add(new cMenuEditIntItem(tr("Brightness"), &newconfig.brightness,-1,0xffff));
#else
  Add(ctrl_hue = new cMenuEditStraItem(tr("HUE"), &newconfig.hue, 33, 
				       controls));
  Add(ctrl_saturation = 
      new cMenuEditStraItem(tr("Saturation"), &newconfig.saturation, 33, 
			    controls));
  Add(ctrl_contrast = 
      new cMenuEditStraItem(tr("Contrast"), &newconfig.contrast, 33, 
			    controls));
  Add(ctrl_brightness = 
      new cMenuEditStraItem(tr("Brightness"), &newconfig.brightness, 33, 
			    controls));
#endif

  //if(current<1) current=1; /* first item is not selectable */
  //SetCurrent(Get(current));
  SetCurrent(Get(1));
  Display();
}

eOSState cMenuSetupVideo::ProcessKey(eKeys Key)
{
  cOsdItem *item = Get(Current());

  eOSState state = cMenuSetupPage::ProcessKey(Key);

  if(Key!=kLeft && Key!=kRight)
    return state;

  if(item == ctrl_hue || item == ctrl_saturation || 
     item == ctrl_contrast || item == ctrl_brightness )
#ifdef INTEGER_CONFIG_VIDEO_CONTROLS
    cXinelibDevice::Instance().ConfigureVideo(newconfig.hue, 
					      newconfig.saturation,
					      newconfig.brightness, 
					      newconfig.contrast);
#else
    cXinelibDevice::Instance().ConfigureVideo(
       INDEX_TO_CONTROL(newconfig.hue), 
       INDEX_TO_CONTROL(newconfig.saturation),
       INDEX_TO_CONTROL(newconfig.brightness), 
       INDEX_TO_CONTROL(newconfig.contrast));
#endif
  return state;
}

void cMenuSetupVideo::Store(void)
{
  memcpy(&xc, &newconfig, sizeof(config_t));

#ifdef INTEGER_CONFIG_VIDEO_CONTROLS
#else
  xc.hue        = INDEX_TO_CONTROL(xc.hue);
  xc.saturation = INDEX_TO_CONTROL(xc.saturation);
  xc.contrast   = INDEX_TO_CONTROL(xc.contrast);
  xc.brightness = INDEX_TO_CONTROL(xc.brightness);
#endif

  SetupStore("Video.HUE",         xc.hue);
  SetupStore("Video.Saturation",  xc.saturation);
  SetupStore("Video.Contrast",    xc.contrast);
  SetupStore("Video.Brightness",  xc.brightness);
}


//--- cMenuSetupOSD ----------------------------------------------------------

class cMenuSetupOSD : public cMenuSetupPage 
{
  private:
    config_t newconfig;

    int orig_alpha_correction;
    int orig_alpha_correction_abs;

    cOsdItem *ctrl_alpha;
    cOsdItem *ctrl_alpha_abs;
    cOsdItem *ctrl_unscaled;
    cOsdItem *ctrl_scale;
    cOsdItem *ctrl_downscale;
    cOsdItem *ctrl_lowres;
  
  protected:
    virtual void Store(void);
    void Set(void);
  
  public:
    cMenuSetupOSD(void);
    ~cMenuSetupOSD();

    virtual eOSState ProcessKey(eKeys Key);
};

cMenuSetupOSD::cMenuSetupOSD(void)
{
  memcpy(&newconfig, &xc, sizeof(config_t));
  orig_alpha_correction     = xc.alpha_correction;
  orig_alpha_correction_abs = xc.alpha_correction_abs;

  Set();
}

cMenuSetupOSD::~cMenuSetupOSD()
{
  xc.alpha_correction = orig_alpha_correction;
  xc.alpha_correction_abs = orig_alpha_correction_abs;

  cXinelibDevice::Instance().ConfigureOSD(xc.prescale_osd, xc.unscaled_osd);
}

void cMenuSetupOSD::Set(void)
{ 
  SetPlugin(cPluginManager::GetPlugin(PLUGIN_NAME_I18N));
  int current = Current();
  Clear();

  ctrl_scale = NULL;
  ctrl_downscale = NULL;
  ctrl_unscaled = NULL;
  ctrl_lowres = NULL;
  ctrl_alpha = NULL;
  ctrl_alpha_abs = NULL;

  Add(NewTitle("On-Screen Display"));
  Add(new cMenuEditBoolItem(tr("Hide main menu"), 
			    &newconfig.hide_main_menu));
  Add(ctrl_scale = 
      new cMenuEditBoolItem(tr("Scale OSD to video size"), 
			    &newconfig.prescale_osd));
  if(newconfig.prescale_osd)
    Add(ctrl_downscale =
	new cMenuEditBoolItem(tr("  Allow downscaling"), 
			      &newconfig.prescale_osd_downscale));
  Add(ctrl_unscaled =
      new cMenuEditBoolItem(tr("Unscaled OSD (no transparency)"), 
			    &newconfig.unscaled_osd));
  if(!newconfig.unscaled_osd) {
    Add(new cMenuEditBoolItem(tr("  When opaque OSD"),
			      &newconfig.unscaled_osd_opaque));
    Add(ctrl_lowres =
	new cMenuEditBoolItem(tr("  When low-res video"),
			      &newconfig.unscaled_osd_lowresvideo));
  }

  Add(ctrl_alpha =
      new cMenuEditTypedIntItem(tr("Dynamic transparency correction"), "%", 
				&newconfig.alpha_correction, -200, 200,
				tr("Off")));
  Add(ctrl_alpha_abs =
      new cMenuEditTypedIntItem(tr("Static transparency correction"), "", 
				&newconfig.alpha_correction_abs, -0xff, 0xff,
				tr("Off")));
  
  if(current<1) current=1; /* first item is not selectable */
  SetCurrent(Get(current));
  SetCurrent(Get(1));
  Display();
}

eOSState cMenuSetupOSD::ProcessKey(eKeys Key)
{
  cOsdItem *item = Get(Current());

  eOSState state = cMenuSetupPage::ProcessKey(Key);

  if(Key!=kLeft && Key!=kRight)
    return state;

  if(item == ctrl_alpha)
    xc.alpha_correction = newconfig.alpha_correction;
  else if(item == ctrl_alpha_abs)
    xc.alpha_correction_abs = newconfig.alpha_correction_abs;
  else if(item == ctrl_unscaled || item == ctrl_scale)
    cXinelibDevice::Instance().ConfigureOSD(newconfig.prescale_osd, 
					    newconfig.unscaled_osd);

  if(newconfig.prescale_osd && !ctrl_downscale)
    Set();
  if(!newconfig.prescale_osd && ctrl_downscale)
    Set();
  if(!newconfig.unscaled_osd && !ctrl_lowres)
    Set();
  if(newconfig.unscaled_osd && ctrl_lowres)
    Set();

  return state;
}

void cMenuSetupOSD::Store(void)
{
  memcpy(&xc, &newconfig, sizeof(config_t));

  orig_alpha_correction = xc.alpha_correction;
  orig_alpha_correction_abs = xc.alpha_correction_abs;

  SetupStore("OSD.HideMainMenu",    xc.hide_main_menu);
  SetupStore("OSD.Prescale",        xc.prescale_osd);
  SetupStore("OSD.Downscale",       xc.prescale_osd_downscale);
  SetupStore("OSD.UnscaledAlways",  xc.unscaled_osd);
  SetupStore("OSD.UnscaledOpaque", xc.unscaled_osd_opaque);
  SetupStore("OSD.UnscaledLowRes", xc.unscaled_osd_lowresvideo);
  SetupStore("OSD.AlphaCorrection", xc.alpha_correction);
  SetupStore("OSD.AlphaCorrectionAbs", xc.alpha_correction_abs);
}


//--- cMenuSetupDecoder ------------------------------------------------------

class cMenuSetupDecoder : public cMenuSetupPage 
{
  private:
    config_t newconfig;

    int pes_buffers_ind;

    cOsdItem *ctrl_pes_buffers_ind;
    cOsdItem *ctrl_pes_buffers;

  protected:
    virtual void Store(void);
    void Set(void);
  
  public:
    cMenuSetupDecoder(void);

    virtual eOSState ProcessKey(eKeys Key);
};

cMenuSetupDecoder::cMenuSetupDecoder(void)
{
  int i;
  memcpy(&newconfig, &xc, sizeof(config_t));

  pes_buffers_ind = PES_BUFFERS_CUSTOM;
  for(i=0;xc.s_bufferSize[i];i++)
    if(xc.pes_buffers == xc.i_pesBufferSize[i])
      pes_buffers_ind = i;

  Set();
}

void cMenuSetupDecoder::Set(void)
{
  SetPlugin(cPluginManager::GetPlugin(PLUGIN_NAME_I18N));
  int current = Current();
  Clear();

  Add(NewTitle("Decoder"));
  Add(new cMenuEditStraI18nItem(tr("Priority"), &xc.decoder_priority, 
				DECODER_PRIORITY_count, xc.s_decoderPriority));
#ifdef ENABLE_SUSPEND
  Add(new cMenuEditTypedIntItem(tr("Stop after inactivity"), tr("min"), 
				&newconfig.inactivity_timer, 0, 1440, 
				tr("Off")));
#endif
  Add(ctrl_pes_buffers_ind = 
      new cMenuEditStraI18nItem(tr("Buffer size"), &pes_buffers_ind, 
				PES_BUFFERS_count, xc.s_bufferSize));
  if(pes_buffers_ind == PES_BUFFERS_CUSTOM)
    Add(ctrl_pes_buffers = 
	new cMenuEditIntItem(tr("  Number of PES packets"), &newconfig.pes_buffers, 
			     10, 10000));
  else
    ctrl_pes_buffers = NULL;

  if(current<1) current=1; /* first item is not selectable */
  SetCurrent(Get(current));
  Display();
}

eOSState cMenuSetupDecoder::ProcessKey(eKeys Key)
{
  cOsdItem *item = Get(Current());

  eOSState state = cMenuSetupPage::ProcessKey(Key);

  if(Key!=kLeft && Key!=kRight)
    return state;

  if(item == ctrl_pes_buffers_ind) {
    if(pes_buffers_ind == PES_BUFFERS_CUSTOM && !ctrl_pes_buffers) {
      Set();
    } else if(pes_buffers_ind != PES_BUFFERS_CUSTOM && ctrl_pes_buffers) {
      Set();
    }
  }

  return state;
}

void cMenuSetupDecoder::Store(void)
{
  int old_buffers = xc.pes_buffers;
  int old_priority = xc.decoder_priority;

  memcpy(&xc, &newconfig, sizeof(config_t));

  if(pes_buffers_ind != PES_BUFFERS_CUSTOM)
    xc.pes_buffers = xc.i_pesBufferSize[pes_buffers_ind];

  SetupStore("Decoder.InactivityTimer", xc.inactivity_timer);
  SetupStore("Decoder.Priority", xc.s_decoderPriority[xc.decoder_priority]);
  SetupStore("Decoder.PesBuffers",    xc.pes_buffers);

  if(xc.pes_buffers != old_buffers || xc.decoder_priority != old_priority)
    cXinelibDevice::Instance().ConfigureDecoder(xc.pes_buffers, 
						xc.decoder_priority);
}


//--- cMenuSetupLocal --------------------------------------------------------

class cMenuSetupLocal : public cMenuSetupPage 
{
  private:
    config_t newconfig;

    int deinterlace;
    int video_driver;
    int local_frontend;

    cOsdItem *ctrl_scale;
    cOsdItem *ctrl_local_fe;
    cOsdItem *ctrl_driver;
    cOsdItem *ctrl_fullscreen;
    cOsdItem *ctrl_window_width;
    cOsdItem *ctrl_window_height;
    cOsdItem *ctrl_deinterlace;
    cOsdItem *ctrl_deinterlace_opts;
    cOsdItem *ctrl_interlace_order;  
    cOsdItem *ctrl_aspect;
    cOsdItem *ctrl_autocrop;

  protected:
    virtual void Store(void);
    void Set(void);
  
  public:
    cMenuSetupLocal(void);
    ~cMenuSetupLocal(void);

    virtual eOSState ProcessKey(eKeys Key);
};

cMenuSetupLocal::cMenuSetupLocal(void)
{
  SetPlugin(cPluginManager::GetPlugin(PLUGIN_NAME_I18N));

  memcpy(&newconfig, &xc, sizeof(config_t));

  local_frontend = strstra(xc.local_frontend, xc.s_frontends, 0);

  video_driver = 0;
  if(local_frontend == FRONTEND_X11)
    video_driver = strstra(xc.video_driver, xc.s_videoDriversX11, 0);
  if(local_frontend == FRONTEND_FB)
    video_driver = strstra(xc.video_driver, xc.s_videoDriversFB, 0);

  deinterlace = strstra(xc.deinterlace_method, xc.s_deinterlaceMethods, 0);

  Set();
}

cMenuSetupLocal::~cMenuSetupLocal(void)
{
  cXinelibDevice::Instance().ConfigureWindow(
       xc.fullscreen, xc.width, xc.height, xc.modeswitch, xc.modeline, 
       xc.display_aspect, xc.scale_video, xc.field_order);
  cXinelibDevice::Instance().ConfigurePostprocessing(
       xc.deinterlace_method, xc.audio_delay, xc.audio_compression, 
       xc.audio_equalizer, xc.audio_surround);
#ifdef ENABLE_TEST_POSTPLUGINS
  cXinelibDevice::Instance().ConfigurePostprocessing(
       "autocrop", xc.autocrop ? true : false, NULL);
#endif
}

void cMenuSetupLocal::Set(void)
{
  int current = Current();
  Clear();

  ctrl_autocrop = NULL;
  ctrl_interlace_order = NULL;
  ctrl_fullscreen = NULL;
  ctrl_window_width  = NULL;
  ctrl_window_height = NULL;
  ctrl_driver = NULL;
  ctrl_aspect = NULL;
  ctrl_scale = NULL;
  ctrl_deinterlace_opts = NULL;

  //Add(NewTitle("Video"));

#ifdef ENABLE_TEST_POSTPLUGINS
#warning move to Video menu (or enable only for local, for remote --> cmdline)
  Add(ctrl_autocrop =
      new cMenuEditBoolItem(tr("Crop letterbox to 16:9"), 
			    &newconfig.autocrop));
#endif

  Add(NewTitle("Local Frontend"));
  Add(ctrl_local_fe = 
      new cMenuEditStraI18nItem(tr("Local Display Frontend"), &local_frontend,
				FRONTEND_count, xc.s_frontendNames));

  if(local_frontend == FRONTEND_X11) {
    Add(ctrl_driver =
	new cMenuEditStraI18nItem(tr("Driver"), &video_driver, 
				  X11_DRIVER_count, 
				  xc.s_videoDriverNamesX11));
    strcpy(newconfig.video_port, "127.0.0.1:0.0");
    Add(new cMenuEditStrItem(tr("Display address"), newconfig.video_port, 
			     31, DriverNameChars));
    Add(new cMenuEditBoolItem(tr("Use keyboard"), 
			      &newconfig.use_x_keyboard));

  } else if(local_frontend == FRONTEND_FB) {
    Add(ctrl_driver =
	new cMenuEditStraI18nItem(tr("Driver"), &video_driver, 
				  FB_DRIVER_count, 
				  xc.s_videoDriverNamesFB));
    strcpy(newconfig.video_port, "/dev/fb/0");
    Add(new cMenuEditStrItem(tr("Framebuffer device"), newconfig.video_port, 31, 
			     DriverNameChars));
  }
#if 0
  if(local_frontend == FRONTEND_FB || !newconfig.fullscreen) {
    Add(new cMenuEditStrItem( "Modeline", newconfig.modeline, 31, 
			      ModeLineChars));
    Add(new cMenuEditBoolItem("Videomode switching", &xc.modeswitch));
  }
#endif

  if(local_frontend == FRONTEND_X11) {
    Add(ctrl_fullscreen = new cMenuEditBoolItem(tr("Fullscreen mode"), 
						&newconfig.fullscreen));
    if(!newconfig.fullscreen) {
      Add(ctrl_window_width = 
	  new cMenuEditTypedIntItem( tr("  Window width"), tr("px"), 
				     &newconfig.width, 1, 2048));
      Add(ctrl_window_height = 
	  new cMenuEditTypedIntItem( tr("  Window height"), tr("px"), 
				     &newconfig.height, 1, 2048));
    }
  }

  if(local_frontend != FRONTEND_NONE) {
    Add(ctrl_aspect = 
	new cMenuEditStraI18nItem(tr("Window aspect"), &newconfig.display_aspect,
				  ASPECT_count, xc.s_aspects));
    Add(ctrl_scale =
	new cMenuEditBoolItem(tr("Scale to window size"), &newconfig.scale_video));

    Add(ctrl_deinterlace = 
	new cMenuEditStraI18nItem(tr("Deinterlacing"), &deinterlace, 
				  DEINTERLACE_count, 
				  xc.s_deinterlaceMethodNames));

    if(deinterlace == DEINTERLACE_TVTIME)
      Add(ctrl_deinterlace_opts = new cMenuEditStrItem(tr("   Options:"), 
						       newconfig.deinterlace_opts, 
						       64, OptionsChars));

#ifdef HAVE_XV_FIELD_ORDER
    if(!deinterlace)
      Add(ctrl_interlace_order = 
	  new cMenuEditStraI18nItem(tr("Interlaced Field Order"), 
				    &newconfig.field_order, FIELD_ORDER_count, 
				    xc.s_fieldOrder));
#endif
  }
  
  if(current<1) current=1; /* first item is not selectable */
  SetCurrent(Get(current));
  Display();
}

eOSState cMenuSetupLocal::ProcessKey(eKeys Key)
{
  int prev_frontend = local_frontend;

  cOsdItem *item = Get(Current());

  eOSState state = cMenuSetupPage::ProcessKey(Key);

  if((Key!=kLeft && Key!=kRight) || !item)
    return state;

  if(item == ctrl_aspect || item == ctrl_scale || item == ctrl_interlace_order)
    cXinelibDevice::Instance().ConfigureWindow(
	xc.fullscreen, xc.width, xc.height, xc.modeswitch, xc.modeline, 
	newconfig.display_aspect, newconfig.scale_video, 
	newconfig.field_order);
  else if(item == ctrl_local_fe && local_frontend != prev_frontend) 
    Set();
  else if(item == ctrl_fullscreen) {
    if(!newconfig.fullscreen && !ctrl_window_width) {
      Set();
    } else if(newconfig.fullscreen && ctrl_window_width) {
      Set();
    }
#ifdef ENABLE_TEST_POSTPLUGINS
  } else if(item == ctrl_autocrop) {
    cXinelibDevice::Instance().ConfigurePostprocessing(
	 "autocrop", xc.autocrop ? true : false, NULL);
#endif
  } else if(item == ctrl_deinterlace) {
    if(deinterlace == DEINTERLACE_TVTIME && !ctrl_deinterlace_opts) {
      Set();
    } else if(deinterlace != DEINTERLACE_TVTIME && ctrl_deinterlace_opts) {
      Set();
    }
  }

#ifdef HAVE_XV_FIELD_ORDER
  else if(item == ctrl_deinterlace) {
    if(!deinterlace && !ctrl_interlace_order) {
      Set();
    } else if(deinterlace && ctrl_interlace_order) {
      Set();
    }
  }
#endif

  return state;
}

void cMenuSetupLocal::Store(void)
{
  memcpy(&xc, &newconfig, sizeof(config_t));

  strcpy(xc.local_frontend, xc.s_frontends[local_frontend]);
  if(local_frontend == FRONTEND_X11)
    strcpy(xc.video_driver, xc.s_videoDriversX11[video_driver]);
  if(local_frontend == FRONTEND_FB)
    strcpy(xc.video_driver, xc.s_videoDriversFB[video_driver]);

  strcpy(xc.deinterlace_method, xc.s_deinterlaceMethods[deinterlace]);

  SetupStore("Frontend", xc.local_frontend);
  SetupStore("Modeline", xc.modeline);
  SetupStore("VideoModeSwitching", xc.modeswitch);
  SetupStore("Fullscreen", xc.fullscreen);
  SetupStore("DisplayAspect", xc.s_aspects[xc.display_aspect]);

  SetupStore("X11.WindowWidth", xc.width);
  SetupStore("X11.WindowHeight", xc.height);
  SetupStore("X11.UseKeyboard", xc.use_x_keyboard);

  SetupStore("Video.Driver", xc.video_driver);
  SetupStore("Video.Port",   xc.video_port);
  SetupStore("Video.Scale",  xc.scale_video);
  SetupStore("Video.Deinterlace", xc.deinterlace_method);
  SetupStore("Video.DeinterlaceOptions", xc.deinterlace_opts);

  SetupStore("Video.FieldOrder",  xc.field_order);
  SetupStore("Video.AutoCrop",   xc.autocrop);
}

//--- cMenuSetupRemote -------------------------------------------------------

class cMenuSetupRemote : public cMenuSetupPage 
{
  private:
    config_t newconfig;

    cOsdItem *ctrl_remote_mode;
    cOsdItem *ctrl_usertp;
    cOsdItem *ctrl_rtp_addr;

  protected:
    virtual void Store(void);
    void Set(void);
  
  public:
    cMenuSetupRemote(void);

    virtual eOSState ProcessKey(eKeys Key);
};

cMenuSetupRemote::cMenuSetupRemote(void)
{
  memcpy(&newconfig, &xc, sizeof(config_t));
  Set();
}

void cMenuSetupRemote::Set(void)
{
  SetPlugin(cPluginManager::GetPlugin(PLUGIN_NAME_I18N));
  Clear();

  Add(NewTitle("Remote Clients"));
  Add(ctrl_remote_mode = new cMenuEditBoolItem(tr("Allow remote clients"), 
					       &newconfig.remote_mode));
  ctrl_usertp = NULL;
  ctrl_rtp_addr = NULL;
  if(newconfig.remote_mode) {
    Add(new cMenuEditIntItem( tr("  Listen port (TCP and broadcast)"), 
			      &newconfig.listen_port,
			      0, 0xffff));
    Add(new cMenuEditBoolItem(tr("  Remote keyboard"), 
			      &newconfig.use_remote_keyboard));

    Add(new cMenuEditBoolItem(tr("  TCP transport"), 
			      &newconfig.remote_usetcp));
    Add(new cMenuEditBoolItem(tr("  UDP transport"), 
			      &newconfig.remote_useudp));
    Add(ctrl_usertp = 
	new cMenuEditBoolItem(tr("  RTP (multicast) transport"), 
			      &newconfig.remote_usertp));
    if(newconfig.remote_usertp) {
      Add(ctrl_rtp_addr =
	  new cMenuEditStrItem( tr("    Multicast address"), 
			        &newconfig.remote_rtp_addr[0], 16, "0123456789."));
      Add(new cMenuEditIntItem( tr("    Multicast port"), 
				&newconfig.remote_rtp_port, 1000, 0xffff));
      Add(new cMenuEditIntItem( tr("    Multicast TTL"), 
				&newconfig.remote_rtp_ttl, 1, 10));
      Add(new cMenuEditBoolItem(tr("    Transmit always on"), 
				&newconfig.remote_rtp_always_on));
    }
    Add(new cMenuEditBoolItem(tr("  PIPE transport"), 
			      &newconfig.remote_usepipe));
    Add(new cMenuEditBoolItem(tr("  Server announce broadcasts"), 
			      &newconfig.remote_usebcast));
  }

  Display();
}

eOSState cMenuSetupRemote::ProcessKey(eKeys Key)
{
  cOsdItem *item = Get(Current());

  eOSState state = cMenuSetupPage::ProcessKey(Key);

  if(Key!=kLeft && Key!=kRight)
    return state;

  if(item == ctrl_remote_mode) {
    if(newconfig.remote_mode && !ctrl_usertp) {
      Set();
    } else if(!newconfig.remote_mode && ctrl_usertp) {
      Set();
    }
  }
  if(item == ctrl_usertp) {
    if(newconfig.remote_usertp && !ctrl_rtp_addr) {
      Set();
    } else if(!newconfig.remote_usertp && ctrl_rtp_addr) {
      Set();
    }
  }

  return state;
}

void cMenuSetupRemote::Store(void)
{
  memcpy(&xc, &newconfig, sizeof(config_t));

  SetupStore("RemoteMode",        xc.remote_mode);
  SetupStore("Remote.ListenPort", xc.listen_port);
  SetupStore("Remote.Keyboard",   xc.use_remote_keyboard);

  SetupStore("Remote.UseTcp", xc.remote_usetcp);
  SetupStore("Remote.UseUdp", xc.remote_useudp);
  SetupStore("Remote.UseRtp", xc.remote_usertp);
  SetupStore("Remote.UsePipe",xc.remote_usepipe);
  SetupStore("Remote.UseBroadcast", xc.remote_usebcast);

  SetupStore("Remote.Rtp.Address",  xc.remote_rtp_addr);
  SetupStore("Remote.Rtp.Port",     xc.remote_rtp_port);
  SetupStore("Remote.Rtp.TTL",      xc.remote_rtp_ttl);
  SetupStore("Remote.Rtp.AlwaysOn", xc.remote_rtp_always_on);

  cXinelibDevice::Instance().Listen(xc.remote_mode, xc.listen_port);
}

} // namespace

//--- cMenuSetupXinelib ------------------------------------------------------

cMenuSetupXinelib::cMenuSetupXinelib(void)
{
  XinelibOutputSetupMenu::controls[0] = tr("Off");
  Set();
}

void cMenuSetupXinelib::Set(void)
{
  Clear();

  SetHasHotkeys();
  Add(new cOsdItem(hk(tr("Audio")),          osUser1));
  Add(new cOsdItem(hk(tr("Audio Equalizer")),osUser2));
  Add(new cOsdItem(hk(tr("Video")),          osUser3));
  Add(new cOsdItem(hk(tr("OSD")),            osUser4));
  Add(new cOsdItem(hk(tr("Decoder")),        osUser5));
  Add(new cOsdItem(hk(tr("Local Frontend")), osUser6));
  Add(new cOsdItem(hk(tr("Remote Clients")), osUser7));

  Display();
}

eOSState cMenuSetupXinelib::ProcessKey(eKeys Key)
{
  eOSState state = cMenuSetupPage::ProcessKey(Key);

  switch (state) {
    case osUser1: 
      return AddSubMenu(new XinelibOutputSetupMenu::cMenuSetupAudio);
    case osUser2: 
      return AddSubMenu(new XinelibOutputSetupMenu::cMenuSetupAudioEq);
    case osUser3: 
      return AddSubMenu(new XinelibOutputSetupMenu::cMenuSetupVideo);
    case osUser4: 
      return AddSubMenu(new XinelibOutputSetupMenu::cMenuSetupOSD);
    case osUser5: 
      return AddSubMenu(new XinelibOutputSetupMenu::cMenuSetupDecoder);
    case osUser6: 
      return AddSubMenu(new XinelibOutputSetupMenu::cMenuSetupLocal);
    case osUser7: 
      return AddSubMenu(new XinelibOutputSetupMenu::cMenuSetupRemote);

    default: ;
  }

  return state;
}


