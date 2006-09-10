/*
 * config.c: User settings
 *
 * See the main source file 'xineliboutput.c' for copyright information and
 * how to reach the author.
 *
 * $Id: config.c,v 1.11 2006-09-10 14:18:50 phintuka Exp $
 *
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>

#include <vdr/config.h>

#include "config.h"

#define DEFAULT_DEINTERLACE_OPTS "method=Linear,cheap_mode=1,pulldown=0,use_progressive_frame_flag=1"

const char *config_t::s_bufferSize[] =
  {"custom","tiny","small","medium","large","huge",NULL};
const int config_t::i_pesBufferSize[] =
  {0,50,250,500,1000,2000,500};
const char *config_t::s_aspects[] =
  {"automatic", "default", "4:3", "16:9", "16:10", "Pan&Scan", "CenterCutOut", 0};
const char *config_t::s_deinterlaceMethods[] =
  {"none", "bob", "weave", "greedy", "onefield", "onefield_xv", 
   "linearblend", "tvtime", 0};
const char *config_t::s_deinterlaceMethodNames[] =
  {"off", "Bob", "Weave", "Greedy", "One Field", "One Field XV", 
   "Linear Blend", "TvTime", NULL};
const char *config_t::s_decoderPriority[] =
  {"low", "normal", "high", 0};
const char *config_t::s_fieldOrder[] =
  {"normal", "inverted", NULL};
const char *config_t::s_audioDriverNames[] =
  {"automatic","Alsa","OSS","no audio","Arts","ESound",NULL};
const char *config_t::s_audioDrivers[] =
  {"auto","alsa","oss","none","arts","esound",NULL};
const char *config_t::s_videoDriverNamesX11[] =
  {"automatic","XShm","Xv","XvMC","XvMC+VLD","no video",NULL};
const char *config_t::s_videoDriversX11[] =
  {"auto","X11","xv","xvmc","xxmc","none",NULL};
const char *config_t::s_videoDriverNamesFB[] =
  {"automatic","Framebuffer","DirectFB","No Video",NULL};
const char *config_t::s_videoDriversFB[] =
  {"auto","fb","DirectFB","none",NULL};
const char *config_t::s_frontendNames[] =
  {"X11 (sxfe)", "Framebuffer (fbfe)", "Off", NULL};
const char *config_t::s_frontends[] =
  {"sxfe", "fbfe", "", NULL};
const char *config_t::s_frontend_files[] =
  {"lib" PLUGIN_NAME_I18N "-sxfe.so." XINELIBOUTPUT_VERSION, 
   "lib" PLUGIN_NAME_I18N "-fbfe.so." XINELIBOUTPUT_VERSION, 
   // example: xineliboutput-sxfe.so.0.4.0
   "", 
   NULL};

const char *config_t::s_audioEqNames[] =
  {"30 Hz", "60 Hz", "125 Hz", "250 Hz", "500 Hz",
   "1 kHz", "2 kHz", "4 kHz", "8 kHz", "16 kHz", NULL};
const char *config_t::s_audioVisualizationNames[] =
  {"Off", "Goom", "Oscilloscope", "FFT Scope", "FFT Graph", NULL};
const char *config_t::s_audioVisualizations[] = 
  {"none", "goom", "oscope", "fftscope", "fftgraph", NULL};

/* xine, audio_alsa_out.c */
const char *config_t::s_speakerArrangements[] =
  {"Mono 1.0", "Stereo 2.0", "Headphones 2.0", "Stereo 2.1",
   "Surround 3.0", "Surround 4.0", "Surround 4.1", 
   "Surround 5.0", "Surround 5.1", "Surround 6.0",
   "Surround 6.1", "Surround 7.1", "Pass Through", 
   NULL};

static char *strcatrealloc(char *dest, const char *src)
{
  if (!src || !*src) 
    return dest;

  int l = (dest ? strlen(dest) : 0) + strlen(src) + 1;
  if(dest) {
    dest = (char *)realloc(dest, l);
    strcat(dest, src);
  } else {
    dest = (char*)malloc(l);
    strcpy(dest, src);
  } 
  return dest;
}

bool config_t::IsVideoFile(const char *fname)
{
  if(fname) {
    char *pos = strrchr(fname,'.');
    if(pos) {
      if(!strcasecmp(pos, ".avi") ||
	 !strcasecmp(pos, ".mpv") ||
	 !strcasecmp(pos, ".vob") || 
	 !strcasecmp(pos, ".vdr") || 
	 !strcasecmp(pos, ".mpg") ||
	 !strcasecmp(pos, ".mpeg")|| 
	 !strcasecmp(pos, ".mpa") || 
	 !strcasecmp(pos, ".mp2") || 
	 !strcasecmp(pos, ".mp3") || 
	 !strcasecmp(pos, ".mp4") || 
	 !strcasecmp(pos, ".asf") || 
	 !strcasecmp(pos, ".flac") || 
	 !strcasecmp(pos, ".ts") || 
	 !strcasecmp(pos, ".xvid") || 
	 !strcasecmp(pos, ".divx") || 
	 !strcasecmp(pos, ".m3u") || 
	 !strcasecmp(pos, ".ram"))
	return true;
    }
  }
  return false;
}

bool config_t::IsImageFile(const char *fname)
{
  if(fname) {
    char *pos = strrchr(fname,'.');
    if(pos) {
      if(!strcasecmp(pos, ".jpg") ||
	 !strcasecmp(pos, ".jpeg") ||
	 !strcasecmp(pos, ".gif") ||
	 !strcasecmp(pos, ".tiff") || 
	 !strcasecmp(pos, ".bmp") || 
	 !strcasecmp(pos, ".png"))
	return true;
    }
  }
  return false;
}

const char *config_t::AutocropOptions(void)
{
  if(autocrop) {
    static char buffer[128];
    sprintf(buffer, "enable_autodetect=%d,soft_start=%d,stabilize=%d,enable_subs_detect=%d",
	    autocrop_autodetect, autocrop_soft, autocrop_fixedsize, autocrop_subs);
    return buffer;
  }
  return NULL;
}

config_t::config_t() {
  memset(this, 0, sizeof(config_t));

  strcpy(local_frontend, s_frontends[FRONTEND_X11]);
  strcpy(video_driver  , s_videoDriversX11[X11_DRIVER_XV]);
  strcpy(video_port    , "0.0");
  strcpy(modeline      , "");

  strcpy(audio_driver , s_audioDrivers[AUDIO_DRIVER_ALSA]);
  strcpy(audio_port   , "default");
  speaker_type = SPEAKERS_STEREO;

  post_plugins = NULL;

  audio_delay       = 0;
  audio_compression = 0;
  memset(audio_equalizer,0,sizeof(audio_equalizer));
  strcpy(audio_visualization, "goom");
  //strcpy(audio_vis_goom_opts, "fps:25,width:720,height:576");
  headphone = 0;
  audio_upmix = 0;
  audio_surround = 0;

  decoder_priority     = DECODER_PRIORITY_NORMAL;
  pes_buffers          = i_pesBufferSize[PES_BUFFERS_SMALL_250];
  strcpy(deinterlace_method, s_deinterlaceMethods[DEINTERLACE_NONE]);
  strcpy(deinterlace_opts, DEFAULT_DEINTERLACE_OPTS);
  display_aspect       = 0;     /* auto */

  hide_main_menu       = 0;
  prescale_osd         = 1;
  prescale_osd_downscale = 0;
  unscaled_osd         = 0;
  unscaled_osd_opaque  = 0;
  unscaled_osd_lowresvideo = 1;

  alpha_correction     = 0;
  alpha_correction_abs = 0;

  fullscreen     = 0;
  modeswitch     = 1;
  width          = 720;
  height         = 576;
  scale_video    = 0;
  field_order    = 0;
  autocrop       = 0;
  autocrop_autodetect = 1;
  autocrop_soft  = 1;
  autocrop_fixedsize = 1;
  autocrop_subs  = 1;

  remote_mode    = 0;
  listen_port    = LISTEN_PORT;
  use_remote_keyboard = 1;
  remote_usetcp   = 1;
  remote_useudp   = 1;
  remote_usertp   = 1;
  remote_usepipe  = 1;
  remote_usebcast = 1;

  strcpy(remote_rtp_addr, "224.0.1.9");
  remote_rtp_port = LISTEN_PORT;
  remote_rtp_ttl = 1;
  remote_rtp_always_on = 0;

  use_x_keyboard = 1;

  hue          = -1; 
  saturation   = -1; 
  contrast     = -1; 
  brightness   = -1; 
  overscan = 0;

  strcpy(browse_files_dir,  "/video");
  strcpy(browse_music_dir,  "/video");
  strcpy(browse_images_dir, "/video");

  main_menu_mode = ShowMenu;
  force_primary_device = 0;

  m_ProcessedArgs = NULL;
};

bool config_t::ProcessArg(const char *Name, const char *Value)
{
  char *s = m_ProcessedArgs;
  m_ProcessedArgs = NULL;
  if(SetupParse(Name, Value)) {
    m_ProcessedArgs = s ? s : strcpy(new char[4096], " ");
    strcat(strcat(m_ProcessedArgs, Name), " ");
    return true;
  }
  m_ProcessedArgs = s;
  return false;
}

bool config_t::ProcessArgs(int argc, char *argv[])
{
  static struct option long_options[] = {
      { "fullscreen",   no_argument,       NULL, 'f' },
      { "width",        required_argument, NULL, 'w' },
      { "height",       required_argument, NULL, 'h' },
      //{ "xkeyboard",    no_argument,       NULL, 'k' },
      //{ "noxkeyboard",  no_argument,       NULL, 'K' },
      { "local",        required_argument, NULL, 'l' },
      //{ "nolocal",      no_argument,       NULL, 'L' },
      //{ "modeline",     required_argument, NULL, 'm' },
      { "remote",       required_argument, NULL, 'r' },
      //{ "noremote",     no_argument,       NULL, 'R' },
      //{ "window",       no_argument,       NULL, 'w' },
      { "audio",        required_argument, NULL, 'A' },
      { "video",        required_argument, NULL, 'V' },
      { "display",      required_argument, NULL, 'd' },
      { "post",         required_argument, NULL, 'P' },
      { "primary",      no_argument,       NULL, 'p' },
      { "exit-on-close",no_argument,     NULL, 'c' },
      { NULL }
    };

  int c;
  while ((c = getopt_long(argc, argv, "fw:h:l:r:A:V:d:P:pc", long_options, NULL)) != -1) {
    switch (c) {
    case 'd': ProcessArg("Video.Port", optarg);
              break;
    case 'f': ProcessArg("Fullscreen", "1");
              break;
    case 'w': ProcessArg("Fullscreen", "0");
              ProcessArg("X11.WindowWidth", optarg);
              break;
    case 'h': ProcessArg("Fullscreen", "0");
              ProcessArg("X11.WindowHeight", optarg);
              break;
    //case 'k': ProcessArg("X11.UseKeyboard", "1");
    //          break;
    //case 'K': ProcessArg("X11.UseKeyboard", "0");
    //          break;
    case 'l': ProcessArg("Frontend", optarg);
              break;
    //case 'L': ProcessArg("Frontend", "none");
    //          break;
    //case 'm': ProcessArg("Modeline", optarg);
    //          break;
    case 'r': if(strcmp(optarg, "none")) {
                ProcessArg("Remote.ListenPort", optarg);
                ProcessArg("RemoteMode", listen_port>0 ? "1" : "0");
              } else
                ProcessArg("RemoteMode", "0");
              break;
    //case 'R': ProcessArg("RemoteMode", "0");
    //          break;
    //case 'w': ProcessArg("Fullscreen", "0");
    //          break;
    case 'V': ProcessArg("Video.Driver", optarg);
              break;
    case 'A': if(strchr(optarg,':')) {
                char *tmp = strdup(optarg);
		char *pt = strchr(tmp,':');
		*pt = 0;
                ProcessArg("Audio.Driver", tmp);
                ProcessArg("Audio.Port", pt+1);
		free(tmp);
              } else
                ProcessArg("Audio.Driver", optarg);
              break;
    case 'P': if(post_plugins)
                post_plugins = strcatrealloc(post_plugins, ";");
              post_plugins = strcatrealloc(post_plugins, optarg);
              break;
    case 'p': ProcessArg("ForcePrimaryDevice", "1");
              break;
    case 'c': exit_on_close = 1;
              break;
    default:  return false;
    }
  }
  return true;
}

bool config_t::SetupParse(const char *Name, const char *Value)
{
  char *pt;
  if(m_ProcessedArgs && NULL != (pt=strstr(m_ProcessedArgs+1, Name)) &&
     *(pt-1) == ' ' && *(pt+strlen(Name)) == ' ')
    return true;

       if (!strcasecmp(Name, "Frontend"))           strcpy(local_frontend, Value);
  else if (!strcasecmp(Name, "Modeline"))           strcpy(modeline, Value);
  else if (!strcasecmp(Name, "VideoModeSwitching")) modeswitch = atoi(Value);
  else if (!strcasecmp(Name, "Fullscreen"))         fullscreen = atoi(Value);
  else if (!strcasecmp(Name, "DisplayAspect"))      display_aspect = strstra(Value, s_aspects, 0);
  else if (!strcasecmp(Name, "ForcePrimaryDevice")) force_primary_device = atoi(Value);

  else if (!strcasecmp(Name, "X11.WindowWidth"))  width = atoi(Value);
  else if (!strcasecmp(Name, "X11.WindowHeight")) height = atoi(Value);
  else if (!strcasecmp(Name, "X11.UseKeyboard"))  use_x_keyboard = atoi(Value);

  else if (!strcasecmp(Name, "Audio.Driver")) strcpy(audio_driver, Value);
  else if (!strcasecmp(Name, "Audio.Port"))   strcpy(audio_port, Value);
  else if (!strcasecmp(Name, "Audio.Speakers")) speaker_type = strstra(Value, s_speakerArrangements, 
								       SPEAKERS_STEREO);
  else if (!strcasecmp(Name, "Audio.Delay"))  audio_delay = atoi(Value);
  else if (!strcasecmp(Name, "Audio.Compression")) audio_compression = atoi(Value);
  else if (!strcasecmp(Name, "Audio.Visualization")) strcpy(audio_visualization, Value);
  else if (!strcasecmp(Name, "Audio.Surround"))  audio_surround = atoi(Value);
  else if (!strcasecmp(Name, "Audio.Upmix"))     audio_upmix = atoi(Value);
  else if (!strcasecmp(Name, "Audio.Headphone")) headphone = atoi(Value);

  else if (!strcasecmp(Name, "OSD.HideMainMenu"))   hide_main_menu = atoi(Value);
  else if (!strcasecmp(Name, "OSD.Prescale"))       prescale_osd = atoi(Value);
  else if (!strcasecmp(Name, "OSD.Downscale"))      prescale_osd_downscale = atoi(Value);
  else if (!strcasecmp(Name, "OSD.UnscaledAlways")) unscaled_osd = atoi(Value);
  else if (!strcasecmp(Name, "OSD.UnscaledOpaque")) unscaled_osd_opaque = atoi(Value);
  else if (!strcasecmp(Name, "OSD.UnscaledLowRes")) unscaled_osd_lowresvideo = atoi(Value);

  else if (!strcasecmp(Name, "OSD.AlphaCorrection"))    alpha_correction = atoi(Value);
  else if (!strcasecmp(Name, "OSD.AlphaCorrectionAbs")) alpha_correction_abs = atoi(Value);

  else if (!strcasecmp(Name, "RemoteMode"))        remote_mode = atoi(Value);
  else if (!strcasecmp(Name, "Remote.ListenPort")) listen_port = atoi(Value);
  else if (!strcasecmp(Name, "Remote.Keyboard"))   use_remote_keyboard = atoi(Value);
  else if (!strcasecmp(Name, "Remote.UseTcp"))   remote_usetcp = atoi(Value);
  else if (!strcasecmp(Name, "Remote.UseUdp"))   remote_useudp = atoi(Value);
  else if (!strcasecmp(Name, "Remote.UseRtp"))   remote_usertp = atoi(Value);
  else if (!strcasecmp(Name, "Remote.UsePipe"))  remote_usepipe= atoi(Value);
  else if (!strcasecmp(Name, "Remote.UseBroadcast")) remote_usebcast = atoi(Value);

  else if (!strcasecmp(Name, "Remote.Rtp.Address"))  strncpy(remote_rtp_addr, Value, 20);
  else if (!strcasecmp(Name, "Remote.Rtp.Port"))     remote_rtp_port = atoi(Value);
  else if (!strcasecmp(Name, "Remote.Rtp.TTL"))      remote_rtp_ttl = atoi(Value);
  else if (!strcasecmp(Name, "Remote.Rtp.AlwaysOn")) remote_rtp_always_on = atoi(Value);

  else if (!strcasecmp(Name, "Decoder.Priority"))        decoder_priority=strstra(Value,s_decoderPriority,1);
  else if (!strcasecmp(Name, "Decoder.PesBuffers"))      pes_buffers=atoi(Value);

  else if (!strcasecmp(Name, "Video.Driver"))      strcpy(video_driver, Value);
  else if (!strcasecmp(Name, "Video.Port"))        strcpy(video_port, Value);
  else if (!strcasecmp(Name, "Video.Scale"))       scale_video = atoi(Value);
  else if (!strcasecmp(Name, "Video.DeinterlaceOptions")) strcpy(deinterlace_opts, Value);
  else if (!strcasecmp(Name, "Video.Deinterlace")) strcpy(deinterlace_method, Value);
  else if (!strcasecmp(Name, "Video.FieldOrder"))  field_order=atoi(Value)?1:0;
  else if (!strcasecmp(Name, "Video.AutoCrop"))    autocrop = atoi(Value);
  else if (!strcasecmp(Name, "Video.AutoCrop.AutoDetect"))   autocrop_autodetect = atoi(Value);
  else if (!strcasecmp(Name, "Video.AutoCrop.SoftStart"))    autocrop_soft = atoi(Value);
  else if (!strcasecmp(Name, "Video.AutoCrop.FixedSize"))    autocrop_fixedsize = atoi(Value);
  else if (!strcasecmp(Name, "Video.AutoCrop.DetectSubs"))   autocrop_subs = atoi(Value);
  else if (!strcasecmp(Name, "Video.HUE"))         hue = atoi(Value);
  else if (!strcasecmp(Name, "Video.Saturation"))  saturation = atoi(Value);
  else if (!strcasecmp(Name, "Video.Contrast"))    contrast = atoi(Value);
  else if (!strcasecmp(Name, "Video.Brightness"))  brightness = atoi(Value);
  else if (!strcasecmp(Name, "Video.Overscan"))    overscan = atoi(Value);

  else if (!strcasecmp(Name, "BrowseFilesDir"))    strcpy(browse_files_dir, Value);
  else if (!strcasecmp(Name, "BrowseMusicDir"))    strcpy(browse_music_dir, Value);
  else if (!strcasecmp(Name, "BrowseImagesDir"))   strcpy(browse_images_dir, Value);

  else if (!strcasecmp(Name, "Audio.Equalizer")) 
    sscanf(Value,"%d %d %d %d %d %d %d %d %d %d",
	   audio_equalizer  ,audio_equalizer+1,
	   audio_equalizer+2,audio_equalizer+3,
	   audio_equalizer+4,audio_equalizer+5,
	   audio_equalizer+6,audio_equalizer+7,
	   audio_equalizer+8,audio_equalizer+9);

  else return false;

  return true;
}

/* Global instance */
config_t xc;


