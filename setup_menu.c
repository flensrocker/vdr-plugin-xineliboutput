/*
 * setup_menu.c: Setup Menu
 *
 * See the main source file 'xineliboutput.c' for copyright information and
 * how to reach the author.
 *
 * $Id: setup_menu.c,v 1.12 2006-09-20 06:59:56 phintuka Exp $
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

#define ISNUMBERKEY(k) (RAWKEY(k) >= k0 && RAWKEY(k) <= k9)

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
    int visualization;

    cOsdItem *audio_ctrl_speakers;
    cOsdItem *audio_ctrl_delay;
    cOsdItem *audio_ctrl_compression;
    cOsdItem *audio_ctrl_upmix;
    cOsdItem *audio_ctrl_surround;
    cOsdItem *audio_ctrl_headphone;
  
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
  visualization = strstra(xc.audio_visualization, 
			  xc.s_audioVisualizations, 
			  0);
  Set();
}

cMenuSetupAudio::~cMenuSetupAudio(void) 
{
  cXinelibDevice::Instance().ConfigurePostprocessing(
        xc.deinterlace_method, xc.audio_delay, xc.audio_compression, 
	xc.audio_equalizer, xc.audio_surround, xc.speaker_type);
  cXinelibDevice::Instance().ConfigurePostprocessing(
        "upmix", xc.audio_upmix ? true : false, NULL);
#ifdef ENABLE_TEST_POSTPLUGINS
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

  Add(audio_ctrl_speakers =
      new cMenuEditStraI18nItem(tr("Speakers"), &newconfig.speaker_type, 
				SPEAKERS_count, xc.s_speakerArrangements));
  Add(audio_ctrl_delay = 
      new cMenuEditTypedIntItem(tr("Delay"), tr("ms"), &newconfig.audio_delay,
				-3000, 3000, tr("Off")));
  Add(audio_ctrl_compression = 
      new cMenuEditTypedIntItem(tr("Audio Compression"), "%", 
				&newconfig.audio_compression, 
				100, 500, NULL, tr("Off")));
  Add(audio_ctrl_upmix =
      new cMenuEditBoolItem(tr("Upmix stereo to 5.1"), 
			    &newconfig.audio_upmix));
  Add(audio_ctrl_surround =
      new cMenuEditBoolItem(tr("Downmix AC3 to surround"), 
			    &newconfig.audio_surround));
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

  eOSState state = cMenuSetupPage::ProcessKey(Key);

  Key = NORMALKEY(Key);

  if(Key!=kLeft && Key!=kRight)
    return state;

  if(item == audio_ctrl_delay || item == audio_ctrl_compression) {
    cXinelibDevice::Instance().ConfigurePostprocessing(
	  xc.deinterlace_method, newconfig.audio_delay, 
	  newconfig.audio_compression, newconfig.audio_equalizer,
	  newconfig.audio_surround, newconfig.speaker_type);
  }
  else if(item == audio_ctrl_speakers) {
    cXinelibDevice::Instance().ConfigurePostprocessing(
          xc.deinterlace_method, newconfig.audio_delay, 
	  newconfig.audio_compression, newconfig.audio_equalizer,
	  newconfig.audio_surround, newconfig.speaker_type);
    if(newconfig.speaker_type <= SPEAKERS_STEREO &&
       newconfig.audio_upmix) {
      newconfig.audio_upmix = false;
      Set();
    }
  }
  else if(item == audio_ctrl_surround) {
    cXinelibDevice::Instance().ConfigurePostprocessing(
          xc.deinterlace_method, newconfig.audio_delay, 
	  newconfig.audio_compression, newconfig.audio_equalizer,
	  newconfig.audio_surround, newconfig.speaker_type);
    if(newconfig.audio_surround && newconfig.audio_upmix) {
      newconfig.audio_upmix = 0;
      Set();
    }
  }
  else if(item == audio_ctrl_upmix) {
    cXinelibDevice::Instance().ConfigurePostprocessing(
	  "upmix", newconfig.audio_upmix ? true : false, NULL);
    if(newconfig.audio_upmix && newconfig.audio_surround) {
      newconfig.audio_surround = 0;
      Set();
    }
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
  strcpy(xc.audio_visualization, xc.s_audioVisualizations[visualization]);

  SetupStore("Audio.Speakers", xc.s_speakerArrangements[xc.speaker_type]);
  SetupStore("Audio.Delay",    xc.audio_delay);
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
	xc.audio_equalizer, xc.audio_surround, xc.speaker_type);
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

  Key = NORMALKEY(Key);

  if(Key == kLeft || Key == kRight) {
    cXinelibDevice::Instance().ConfigurePostprocessing(
	xc.deinterlace_method, xc.audio_delay, xc.audio_compression, 
	newconfig.audio_equalizer, xc.audio_surround, xc.speaker_type);
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

static const char *tvtime_method[] =
  { "use_vo_driver",
    "Linear",
    "LinearBlend",
    "Greedy",
    "Greedy2Frame",
    "Weave",
    "LineDoubler",
    "Vertical",
    "ScalerBob",
    "GreedyH",
    "TomsMoComp",
    NULL};

static const int tvtime_methods_count = (sizeof(tvtime_method)-1)/sizeof(char*);

static const char *tvtime_method_name[] =
  {"Use Video-Out Driver",               // "use_vo_driver"
   "Linear Interpolation",               // "Linear",
   "Linear Blend (mplayer)",             // "LinearBlend",
   "Greedy - Low motion (DScaler)",      // "Greedy",
   "Greedy 2-frame (DScaler)",           // "Greedy2Frame",
   "Weave Last Field",                   // "Weave",
   "Line Doubler",                       // "LineDoubler",
   "Vertical Blend (ffmpeg)",            // "Vertical",
   "Scaler Bob",                         // "ScalerBob",
   "Greedy - High Motion (DScaler)",     // "GreedyH",
   "Tom's Motion Compensated (DScaler)", // "TomsMoComp",
   NULL};

static const char *tvtime_pulldown[] = 
  { "none",
    "vector",
    NULL};

static const char *tvtime_pulldown_name[] = 
  { "None",
    "Vector",
    NULL};

static const char *tvtime_framerate[] = 
  { "full",
    "half_top",
    "half_bottom",
    NULL};

static const char *tvtime_framerate_name[] = 
  { "Full",
    "Half (top)",
    "Half (bottom)",
    NULL};

struct tvtime_s {
  int method;
  int cheap_mode;        // on/off
  int pulldown;          // none, vector
  int framerate;         // full, half_top, half_bottom
  int judder_correction; // on/off
  int use_progressive_frame_flag; // on/off
  int chroma_filter;     // on/off

  void Parse(const char *str)
  {
    cheap_mode = strstr(str, "cheap_mode=1") ? 1 : 0;
    pulldown   = strstr(str, "pulldown=none") ? 0 :
                 strstr(str, "pulldown=0") ? 0 : 1;
    framerate  = strstr(str, "framerate_mode=half_top")    ? 1 : 
                 strstr(str, "framerate_mode=1")           ? 1 : 
                 strstr(str, "framerate_mode=half_bottom") ? 2 :
                 strstr(str, "framerate_mode=2")           ? 2 : 0;
    chroma_filter = strstr(str, "chroma_filter=1") ? 1 : 0;
    judder_correction = strstr(str, "judder_correction=0") ? 0 : 1;
    use_progressive_frame_flag = strstr(str, "use_progressive_frame_flag=0") ? 0 : 1;
    method=1;
    char *m = strstr(str, "method=");
    if(m) {
      char *tmp = strdup(m + 7);
      if(strchr(tmp, ','))
	*strchr(tmp, ',') = 0;
      method = strstra(tmp, tvtime_method, 1);
      free(tmp);
    }
  }

  const char *ToString(void)
  {
    static char buf[256];
    sprintf(buf, "method=%s,cheap_mode=%d,pulldown=%s,framerate=%s,"
	    "judder_correction=%d,use_progressive_frame_flag=%d,"
	    "chroma_filter=%d",
	    tvtime_method[method], cheap_mode, tvtime_pulldown[pulldown],
	    tvtime_framerate[framerate], judder_correction, 
	    use_progressive_frame_flag, chroma_filter);
    return buf;
  }
};

class cMenuSetupVideo : public cMenuSetupPage 
{
  private:
    config_t newconfig;

    cOsdItem *ctrl_autocrop;
    cOsdItem *ctrl_hue;
    cOsdItem *ctrl_saturation;
    cOsdItem *ctrl_contrast;
    cOsdItem *ctrl_brightness;
    cOsdItem *ctrl_overscan;
    cOsdItem *ctrl_pp;
    cOsdItem *ctrl_deinterlace;
    cOsdItem *ctrl_deinterlace_opts;

    int deinterlace;
    struct tvtime_s tvtime;
  
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

  deinterlace = strstra(xc.deinterlace_method, xc.s_deinterlaceMethods, 0);

  tvtime.Parse(newconfig.deinterlace_opts);

  Set();
}

cMenuSetupVideo::~cMenuSetupVideo(void)
{
  cXinelibDevice::Instance().ConfigureVideo(xc.hue, xc.saturation, 
					    xc.brightness, xc.contrast,
					    xc.overscan);
  cXinelibDevice::Instance().ConfigurePostprocessing(
       "autocrop", xc.autocrop ? true : false, xc.AutocropOptions());
  cXinelibDevice::Instance().ConfigurePostprocessing(
       "pp", xc.ffmpeg_pp ? true : false, xc.FfmpegPpOptions());
}

void cMenuSetupVideo::Set(void)
{
  SetPlugin(cPluginManager::GetPlugin(PLUGIN_NAME_I18N));
  int current = Current();
  Clear();

  Add(NewTitle("Video"));

  Add(ctrl_autocrop = 
      new cMenuEditBoolItem(tr("Crop letterbox 4:3 to 16:9"), 
			    &newconfig.autocrop));
  if(newconfig.autocrop) {
    Add(new cMenuEditBoolItem(tr("  Autodetect letterbox"), 
			      &newconfig.autocrop_autodetect));
    Add(new cMenuEditBoolItem(tr("  Soft start"), 
			      &newconfig.autocrop_soft));
    Add(new cMenuEditBoolItem(tr("  Crop to"),
			      &newconfig.autocrop_fixedsize,
			      "4:3...20:9", "14:9/16:9"));
    Add(new cMenuEditBoolItem(tr("  Detect subtitles"),
			      &newconfig.autocrop_subs));
  }

  Add(ctrl_overscan =
      new cMenuEditTypedIntItem(tr("Overscan (crop image borders)"), "%", 
				&newconfig.overscan, 0, 10,
				tr("Off")));

  Add(ctrl_pp = new cMenuEditBoolItem(tr("Post processing (ffmpeg)"), 
				      &newconfig.ffmpeg_pp));
  if(newconfig.ffmpeg_pp) {
    Add(new cMenuEditIntItem( tr("  Quality"), 
			      &newconfig.ffmpeg_pp_quality, 0, 6));
    Add(new cMenuEditStrItem( tr("  Mode"), newconfig.ffmpeg_pp_mode, 
			      255, OptionsChars));
  }

  Add(ctrl_deinterlace = 
      new cMenuEditStraI18nItem(tr("Deinterlacing"), &deinterlace, 
				DEINTERLACE_count, 
				xc.s_deinterlaceMethodNames));

  ctrl_deinterlace_opts = NULL;
  if(deinterlace == DEINTERLACE_TVTIME) {
    Add(ctrl_deinterlace_opts = new cMenuEditStrItem(tr("   Options"), 
						     newconfig.deinterlace_opts, 
						     254, OptionsChars));

    Add(new cMenuEditStraI18nItem(tr("     Method"), &tvtime.method,
				  tvtime_methods_count, tvtime_method_name));
    Add(new cMenuEditBoolItem(tr("     Cheap mode"), &tvtime.cheap_mode));
    Add(new cMenuEditStraI18nItem(tr("     Pulldown"), &tvtime.pulldown,
				  2, tvtime_pulldown_name));
    Add(new cMenuEditStraI18nItem(tr("     Frame rate"), &tvtime.framerate,
				  3, tvtime_framerate_name));
    Add(new cMenuEditBoolItem(tr("     Judder Correction"), &tvtime.judder_correction));
    Add(new cMenuEditBoolItem(tr("     Use progressive frame flag"), 
			      &tvtime.use_progressive_frame_flag));
    Add(new cMenuEditBoolItem(tr("     Chroma Filter"), 
			      &tvtime.chroma_filter));
  }

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

  if(current<1) current=1; /* first item is not selectable */
  SetCurrent(Get(current));
  //SetCurrent(Get(1));
  Display();
}

eOSState cMenuSetupVideo::ProcessKey(eKeys Key)
{
  cOsdItem *item = Get(Current());

  eOSState state = cMenuSetupPage::ProcessKey(Key);

  Key = NORMALKEY(Key);

  if(Key!=kLeft && Key!=kRight)
    return state;

  if(item == ctrl_hue || item == ctrl_saturation || 
     item == ctrl_contrast || item == ctrl_brightness ||
     item == ctrl_overscan)
#ifdef INTEGER_CONFIG_VIDEO_CONTROLS
    cXinelibDevice::Instance().ConfigureVideo(newconfig.hue, 
					      newconfig.saturation,
					      newconfig.brightness, 
					      newconfig.contrast,
					      newconfig.overscan);
#else
    cXinelibDevice::Instance().ConfigureVideo(
       INDEX_TO_CONTROL(newconfig.hue), 
       INDEX_TO_CONTROL(newconfig.saturation),
       INDEX_TO_CONTROL(newconfig.brightness), 
       INDEX_TO_CONTROL(newconfig.contrast),
       newconfig.overscan);
#endif
  else if(item == ctrl_autocrop) {
    cXinelibDevice::Instance().ConfigurePostprocessing(
	 "autocrop", newconfig.autocrop ? true : false, 
	 newconfig.AutocropOptions());
    Set();
  }
  else if(item == ctrl_pp) {
    cXinelibDevice::Instance().ConfigurePostprocessing(
	 "pp", newconfig.ffmpeg_pp ? true : false, 
	 newconfig.FfmpegPpOptions());
    Set();
  } else if(item == ctrl_deinterlace) {
    if(deinterlace == DEINTERLACE_TVTIME && !ctrl_deinterlace_opts) {
      Set();
    } else if(deinterlace != DEINTERLACE_TVTIME && ctrl_deinterlace_opts) {
      Set();
    }
  }

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

  strcpy(xc.deinterlace_method, xc.s_deinterlaceMethods[deinterlace]);
  strcpy(xc.deinterlace_opts, tvtime.ToString());
  SetupStore("Video.Deinterlace", xc.deinterlace_method);
  SetupStore("Video.DeinterlaceOptions", xc.deinterlace_opts);

  SetupStore("Video.AutoCrop",   xc.autocrop); 
  SetupStore("Video.AutoCrop.AutoDetect", xc.autocrop_autodetect);
  SetupStore("Video.AutoCrop.SoftStart",  xc.autocrop_soft);
  SetupStore("Video.AutoCrop.FixedSize",  xc.autocrop_fixedsize);
  SetupStore("Video.AutoCrop.DetectSubs", xc.autocrop_subs);
  SetupStore("Video.HUE",        xc.hue);
  SetupStore("Video.Saturation", xc.saturation);
  SetupStore("Video.Contrast",   xc.contrast);
  SetupStore("Video.Brightness", xc.brightness);
  SetupStore("Video.Overscan",   xc.overscan);
  SetupStore("Post.pp.Enable",   xc.ffmpeg_pp);
  SetupStore("Post.pp.Quality",  xc.ffmpeg_pp_quality);
  SetupStore("Post.pp.Mode",     xc.ffmpeg_pp_mode);
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
  //SetCurrent(Get(1));
  Display();
}

eOSState cMenuSetupOSD::ProcessKey(eKeys Key)
{
  cOsdItem *item = Get(Current());

  eOSState state = cMenuSetupPage::ProcessKey(Key);

  Key = NORMALKEY(Key);

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

  Key = NORMALKEY(Key);

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

    int local_frontend;
    int audio_driver;
    int video_driver;

    cOsdItem *ctrl_scale;
    cOsdItem *ctrl_local_fe;
    cOsdItem *ctrl_driver;
    cOsdItem *ctrl_fullscreen;
    cOsdItem *ctrl_window_width;
    cOsdItem *ctrl_window_height;
    cOsdItem *ctrl_interlace_order;  
    cOsdItem *ctrl_aspect;
    cOsdItem *ctrl_audio_driver;
    cOsdItem *ctrl_audio_port;

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
  audio_driver  = strstra(xc.audio_driver, xc.s_audioDrivers, 0);

  video_driver = 0;
  if(local_frontend == FRONTEND_X11)
    video_driver = strstra(xc.video_driver, xc.s_videoDriversX11, 0);
  if(local_frontend == FRONTEND_FB)
    video_driver = strstra(xc.video_driver, xc.s_videoDriversFB, 0);

  Set();
}

cMenuSetupLocal::~cMenuSetupLocal(void)
{
  cXinelibDevice::Instance().ConfigureWindow(
       xc.fullscreen, xc.width, xc.height, xc.modeswitch, xc.modeline, 
       xc.display_aspect, xc.scale_video, xc.field_order);
  cXinelibDevice::Instance().ConfigurePostprocessing(
       xc.deinterlace_method, xc.audio_delay, xc.audio_compression, 
       xc.audio_equalizer, xc.audio_surround, xc.speaker_type);
}

void cMenuSetupLocal::Set(void)
{
  int current = Current();
  Clear();

  ctrl_interlace_order = NULL;
  ctrl_fullscreen = NULL;
  ctrl_window_width  = NULL;
  ctrl_window_height = NULL;
  ctrl_driver = NULL;
  ctrl_aspect = NULL;
  ctrl_scale = NULL;
  ctrl_audio_driver = NULL;
  ctrl_audio_port = NULL;

  Add(NewTitle("Local Frontend"));

  Add(ctrl_local_fe = 
      new cMenuEditStraI18nItem(tr("Local Display Frontend"), &local_frontend,
				FRONTEND_count, xc.s_frontendNames));

  if(local_frontend == FRONTEND_X11) {
    Add(new cMenuEditBoolItem(tr("Use keyboard"), 
			      &newconfig.use_x_keyboard));
  }

  if(local_frontend != FRONTEND_NONE) {
    Add(NewTitle("Video"));
  }

  if(local_frontend == FRONTEND_X11) {
    Add(ctrl_driver =
	new cMenuEditStraI18nItem(tr("Driver"), &video_driver, 
				  X11_DRIVER_count, 
				  xc.s_videoDriverNamesX11));
    strcpy(newconfig.video_port, "0.0");
    Add(new cMenuEditStrItem(tr("Display address"), newconfig.video_port, 
			     31, DriverNameChars));

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

#ifdef HAVE_XV_FIELD_ORDER
    Add(ctrl_interlace_order = 
	new cMenuEditStraI18nItem(tr("Interlaced Field Order"), 
				  &newconfig.field_order, FIELD_ORDER_count, 
				  xc.s_fieldOrder));
#endif

    Add(NewTitle("Audio"));

    Add(ctrl_audio_driver = 
	new cMenuEditStraI18nItem(tr("Driver"), &audio_driver, 
				  AUDIO_DRIVER_count, xc.s_audioDriverNames));
    if(audio_driver != AUDIO_DRIVER_AUTO && audio_driver != AUDIO_DRIVER_NONE)
      Add(ctrl_audio_port = 
	  new cMenuEditStrItem(tr("Port"), newconfig.audio_port, 31, 
			       DriverNameChars));

  }
  
  if(current<1) current=1; /* first item is not selectable */
  SetCurrent(Get(current));
  Display();
}

eOSState cMenuSetupLocal::ProcessKey(eKeys Key)
{
  int prev_frontend = local_frontend;
  int prev_audio_driver = audio_driver;

  cOsdItem *item = Get(Current());

  eOSState state = cMenuSetupPage::ProcessKey(Key);

  Key = NORMALKEY(Key);

  if((Key!=kLeft && Key!=kRight) || !item)
    return state;

  if(item == ctrl_audio_driver) {
    if(prev_audio_driver != audio_driver) {
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
	    !ctrl_audio_port)
      Set();
    else if((audio_driver == AUDIO_DRIVER_AUTO || 
	     audio_driver == AUDIO_DRIVER_NONE) && 
	    ctrl_audio_port)
      Set();
  }
  else if(item == ctrl_aspect || item == ctrl_scale || item == ctrl_interlace_order)
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
  }

  return state;
}

void cMenuSetupLocal::Store(void)
{
  memcpy(&xc, &newconfig, sizeof(config_t));

  strcpy(xc.audio_driver, xc.s_audioDrivers[audio_driver]);
  strcpy(xc.local_frontend, xc.s_frontends[local_frontend]);
  if(local_frontend == FRONTEND_X11)
    strcpy(xc.video_driver, xc.s_videoDriversX11[video_driver]);
  if(local_frontend == FRONTEND_FB)
    strcpy(xc.video_driver, xc.s_videoDriversFB[video_driver]);

  SetupStore("Frontend",      xc.local_frontend);
  SetupStore("Audio.Driver",  xc.audio_driver);
  SetupStore("Audio.Port",    xc.audio_port);
  SetupStore("Video.Driver",  xc.video_driver);
  SetupStore("Video.Port",    xc.video_port);
  SetupStore("Video.Scale",   xc.scale_video);
  SetupStore("Video.FieldOrder", xc.field_order);
  SetupStore("Modeline",      xc.modeline);
  SetupStore("VideoModeSwitching", xc.modeswitch);
  SetupStore("Fullscreen",    xc.fullscreen);
  SetupStore("DisplayAspect", xc.s_aspects[xc.display_aspect]);
  SetupStore("X11.WindowWidth",  xc.width);
  SetupStore("X11.WindowHeight", xc.height);
  SetupStore("X11.UseKeyboard",  xc.use_x_keyboard);
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
  int current = Current();
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

  if(current<1) current=1; /* first item is not selectable */
  SetCurrent(Get(current));
  Display();
}

eOSState cMenuSetupRemote::ProcessKey(eKeys Key)
{
  cOsdItem *item = Get(Current());

  eOSState state = cMenuSetupPage::ProcessKey(Key);

  Key = NORMALKEY(Key);

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


//--- cMenuTestImages ------------------------------------------------------

#include <vdr/osdbase.h>

#define OSD_W (720-2)
#define OSD_H (576-2)
#define OSD_X (1)
#define OSD_Y (1)

//
// cTestGrayscale
//

class cTestGrayscale : public cOsdObject 
{
  private:
    cOsd *m_Osd;

  public:
    cTestGrayscale() { m_Osd = NULL; }
    virtual ~cTestGrayscale() { delete m_Osd; }

    virtual void Show();
    virtual eOSState ProcessKey(eKeys Key);
};

void cTestGrayscale::Show()
{
  tArea areas [] = { {      0, 0, OSD_W/2 - 1, OSD_H - 1, 8},
                     {OSD_W/2, 0,   OSD_W - 1, OSD_H - 1, 8}};
  int i;

  if(!m_Osd)
    m_Osd = cOsdProvider::NewOsd(OSD_X, OSD_Y);

  if(m_Osd) {
    if (m_Osd->CanHandleAreas(areas, sizeof(areas) / sizeof(tArea) ) == oeOk) {
      m_Osd->SetAreas(areas, sizeof(areas) / sizeof(tArea));
      m_Osd->Flush();

      // border
      m_Osd->DrawRectangle(0, 0, OSD_W - 1, OSD_H - 1, 0xff000000);
      m_Osd->DrawRectangle(1, 1, OSD_W - 2, OSD_H - 2, 0xff000000);

      // background
      m_Osd->DrawRectangle(2, 2, 2+103, OSD_H - 3, 0xffffffff); 
      m_Osd->DrawRectangle(OSD_W-2-103, 2, OSD_W-2, OSD_H - 3, 0xff000000); 

      for(i=0; i<0xff; i++)
	m_Osd->DrawRectangle(2+103+2*i, 2, 2+103+2*(i+1), OSD_H - 3, 
			     0xff000000|(i*0x00010101)/*=(i<<16)|(i<<8)|(i)*/);
      // line
      m_Osd->DrawRectangle(1, OSD_H/2-20, OSD_W - 2, OSD_H/2, 0xffffffff);
      m_Osd->DrawRectangle(1, OSD_H/2+1, OSD_W - 2, OSD_H/2+21, 0xff000000);

      // Cross
      for(int x=0; x<OSD_W;x++) {
	m_Osd->DrawPixel(x, x*OSD_H/OSD_W, 0x00000000);
	m_Osd->DrawPixel(x, OSD_H - 1 - x*OSD_H/OSD_W, 0x00000000);
      }

      // commit
      m_Osd->Flush();
    }

  }
}

eOSState cTestGrayscale::ProcessKey(eKeys key)
{
  char s[32];
  static int br = xc.brightness;
  static int co = xc.contrast;
  eOSState state = cOsdObject::ProcessKey(key);
  if (state == osUnknown) {
    switch (key & ~k_Repeat) {
      case kOk:
      case kBack:
	return osEnd;
      case kRight:
	br += 0xffff/1024*2;
      case kLeft:
	br -= 0xffff/1024;
	sprintf(s, "b %d", br);
	m_Osd->DrawText(400, 100, s, 0xff000000, 0xffffffff, cFont::GetFont(fontSml));
	cXinelibDevice::Instance().ConfigureVideo(xc.hue, xc.saturation, br, co, xc.overscan);
	m_Osd->Flush();
	return osContinue;	
      case kUp:
	co += 0xffff/1024*2;
      case kDown:
	co -= 0xffff/1024;
	sprintf(s, "c %d", co);
	m_Osd->DrawText(400, 130, s, 0xff000000, 0xffffffff, cFont::GetFont(fontSml));
	cXinelibDevice::Instance().ConfigureVideo(xc.hue, xc.saturation, br, co, xc.overscan);
	m_Osd->Flush();
	return osContinue;
    }
  }
  return state;
}


//
// cTestBitmap
//

class cTestBitmap : public cOsdObject 
{
  private:
    cOsd *m_Osd;
    int bpp;

  public:
    cTestBitmap(int _bpp = 1) { 
      m_Osd = NULL; 
      if(_bpp<1) _bpp = 1; 
      if(_bpp>6) _bpp = 6; 
      bpp = 1<<_bpp; 
    }
    virtual ~cTestBitmap() { delete m_Osd; }

    virtual void Show();
    virtual eOSState ProcessKey(eKeys Key);
};

void cTestBitmap::Show()
{
  tArea areas [] = {{ 0, 0, OSD_W - 1, OSD_H - 1, 8}};
  int x, y, bit = 0;

  if(!m_Osd) {
    m_Osd = cOsdProvider::NewOsd(OSD_X, OSD_Y);

    if(m_Osd) {
      if (m_Osd->CanHandleAreas(areas, sizeof(areas) / sizeof(tArea) ) == oeOk) {
	m_Osd->SetAreas(areas, sizeof(areas) / sizeof(tArea));
	m_Osd->Flush();
      }
    }      
  }
   
  if(m_Osd) {
    for(x=0; x<OSD_W; x+=bpp) {
      bit = (x/bpp) & 1;
      for(y=0; y<OSD_H; y+=bpp) {
	m_Osd->DrawRectangle(x, y, x+bpp, y+bpp, bit?0xffffffff:0xff000000);
	bit = !bit;
      }
    }
    // commit
    m_Osd->Flush();
  }
}

eOSState cTestBitmap::ProcessKey(eKeys key)
{
  eOSState state = cOsdObject::ProcessKey(key);

  if (state == osUnknown) {
    switch (key & ~k_Repeat) {
      case kOk:
      case kBack:
	return osEnd;
      case kRight:
	bpp = (bpp<64) ? (bpp<<1) : 1;
	Show();
	return osContinue;
      case kLeft:
	bpp = (bpp>1) ? (bpp>>1) : 64;
	Show();
	return osContinue;	
      default:
	break;
    }
  }
  return state;
}

//
// cMenuTestImages
//

#include <vdr/remote.h>   // CallPlugin

extern cOsdObject *g_PendingMenuAction;

class cMenuTestImages : public cMenuSetupPage {

  protected:
    void Set(void);
    virtual void Store(void) {};
 
  public:
    cMenuTestImages();

    virtual eOSState ProcessKey(eKeys Key);
};

cMenuTestImages::cMenuTestImages() 
{ 
  Set();
}

void cMenuTestImages::Set(void)
{
  char buf[128];
  Clear();
  
  SetHasHotkeys();
  Add(new cOsdItem(tr("Grayscale"), osUser1));
  sprintf(buf, "%s 1bit", tr("Bitmap"));
  Add(new cOsdItem(buf, osUser2));
  sprintf(buf, "%s 4bit", tr("Bitmap"));
  Add(new cOsdItem(buf, osUser3));
      
  Display();
}

eOSState cMenuTestImages::ProcessKey(eKeys Key)
{
  eOSState state = cMenuSetupPage::ProcessKey(Key);

  switch (state) {
    case osUser1:
      g_PendingMenuAction = new cTestGrayscale();
      cRemote::CallPlugin("xineliboutput");
      return osEnd;
    case osUser2:
      g_PendingMenuAction = new cTestBitmap(1);
      cRemote::CallPlugin("xineliboutput");
      return osEnd;
    case osUser3:
      g_PendingMenuAction = new cTestBitmap(4);
      cRemote::CallPlugin("xineliboutput");
      return osEnd;
    default: ;
  }

  return state;
}


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
  Add(new cOsdItem(hk(tr("Test Images")),    osUser8));

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
    case osUser8: 
      return AddSubMenu(new cMenuTestImages);

    default: ;
  }

  return state;
}


