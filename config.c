/*
 * config.c: User settings
 *
 * See the main source file 'xineliboutput.c' for copyright information and
 * how to reach the author.
 *
 * $Id: config.c,v 1.30 2007-03-17 15:48:21 phintuka Exp $
 *
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>

#include <vdr/config.h>
#include <vdr/videodir.h>

#include "logdefs.h"
#include "config.h"

#define STRN0CPY(dst, src) \
  do { \
    strn0cpy(dst, src, sizeof(dst)); \
    if(strlen(src) >= sizeof(dst)) \
      LOGMSG("WARNING: Setting %s truncated to %s !", Name, dst); \
  } while(0)


#define DEFAULT_DEINTERLACE_OPTS "method=Linear,cheap_mode=1,pulldown=0,use_progressive_frame_flag=1"

const char *config_t::s_bufferSize[] =
  {"custom", "tiny", "small", "medium", "large", "huge", NULL};
const int config_t::i_pesBufferSize[] =
  {0, 50, 250, 500, 1000, 2000, 500};
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
  {"automatic", "Alsa", "OSS", "no audio", "Arts", "ESD", "Jack", NULL};
const char *config_t::s_audioDrivers[] =
  {"auto",      "alsa", "oss", "none",     "arts", "esd", "jack", NULL};
const char *config_t::s_videoDriverNamesX11[] =
  {"automatic", "XShm", "Xv", "XvMC", "XvMC+VLD", "Vidix", "XDirectFB", "OpenGL", "SDL", "no video",NULL};
const char *config_t::s_videoDriversX11[] =
  {"auto",      "xshm", "xv", "xvmc", "xxmc",     "vidix", "XDirectFB", "opengl", "sdl", "none",    NULL};
const char *config_t::s_videoDriverNamesFB[] =
  {"automatic", "Framebuffer", "DirectFB", "SDL", "VidixFB", "DXR3",   "No Video", NULL};
const char *config_t::s_videoDriversFB[] =
  {"auto",      "fb",          "DirectFB", "sdl", "vidixfb", "aadxr3", "none",     NULL};
const char *config_t::s_frontendNames[] =
  {"X11 (sxfe)", "Framebuffer (fbfe)", "Off", NULL};
const char *config_t::s_frontends[] =
  {"sxfe", "fbfe", "none", NULL};
const char *config_t::s_frontend_files[] =
  {"lib" PLUGIN_NAME_I18N "-sxfe.so." XINELIBOUTPUT_VERSION, 
   "lib" PLUGIN_NAME_I18N "-fbfe.so." XINELIBOUTPUT_VERSION, 
   // example: libxineliboutput-sxfe.so.0.4.0
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

const char *config_t::s_subExts[] = {".sub", ".srt", ".txt", ".ssa", 
				     ".SUB", ".SRT", ".TXT", ".SSA", 
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

bool config_t::IsPlaylistFile(const char *fname)
{
  if(fname) {
    char *pos = strrchr(fname,'.');
    if(pos) {
      pos++;
      if(!strcasecmp(pos, "pls") || 
	 !strcasecmp(pos, "m3u") || 
	 !strcasecmp(pos, "ram") ||
	 !strcasecmp(pos, "asx"))
	return true;
    }
  }
  return false;
}

bool config_t::IsAudioFile(const char *fname)
{
  if(fname) {
    char *pos = strrchr(fname,'.');
    if(pos) {
      pos++;
      if(!strcasecmp(pos, "mpa") ||
	 !strcasecmp(pos, "mp2") ||
	 !strcasecmp(pos, "mp3") ||
	 !strcasecmp(pos, "mpega") ||
	 !strcasecmp(pos, "flac") ||
	 !strcasecmp(pos, "ac3") ||
	 !strcasecmp(pos, "ogg") ||
	 !strcasecmp(pos, "ogm") ||
	 !strcasecmp(pos, "au") ||
	 !strcasecmp(pos, "aud") ||
	 !strcasecmp(pos, "wma") ||
	 !strcasecmp(pos, "asf") ||
	 !strcasecmp(pos, "wav") ||
	 !strcasecmp(pos, "spx") ||
	 !strcasecmp(pos, "ra"))
	return true;
      return IsPlaylistFile(fname);
    }
  }
  return false;
}

bool config_t::IsVideoFile(const char *fname)
{
  if(fname) {
    char *pos = strrchr(fname,'.');
    if(pos) {
      pos++;
      if(!strcasecmp(pos, "avi") ||
	 !strcasecmp(pos, "mpv") ||
	 !strcasecmp(pos, "vob") ||
	 !strcasecmp(pos, "vdr") ||
	 !strcasecmp(pos, "mpg") ||
	 !strcasecmp(pos, "mpeg")||
	 !strcasecmp(pos, "mp4") ||
	 !strcasecmp(pos, "asf") ||
	 !strcasecmp(pos, "wmv") ||
	 !strcasecmp(pos, "mov") ||
	 !strcasecmp(pos, "ts") ||
	 !strcasecmp(pos, "pes") ||
	 !strcasecmp(pos, "xvid") ||
	 !strcasecmp(pos, "divx") ||
	 !strcasecmp(pos, "fli") ||
	 !strcasecmp(pos, "dv") ||
	 !strcasecmp(pos, "dat") ||
	 !strcasecmp(pos, "mkv") ||
	 !strcasecmp(pos, "rm") ||
	 !strcasecmp(pos, "iso"))  /* maybe dvd */
	return true;
      return IsAudioFile(fname);
    }
  }
  return false;
}

bool config_t::IsImageFile(const char *fname)
{
  if(fname) {
    char *pos = strrchr(fname,'.');
    if(pos) {
      pos++;
      if(!strcasecmp(pos, "jpg") ||
	 !strcasecmp(pos, "jpeg") ||
	 !strcasecmp(pos, "gif") ||
	 !strcasecmp(pos, "tiff") || 
	 !strcasecmp(pos, "bmp") || 
	 !strcasecmp(pos, "mng") ||
	 !strcasecmp(pos, "png"))
	return true;
    }
  }
  return false;
}

const char *config_t::AutocropOptions(void)
{
  if(autocrop) {
    static char buffer[256];
    snprintf(buffer, sizeof(buffer),
	     "enable_autodetect=%d,soft_start=%d,stabilize=%d,enable_subs_detect=%d",
	     autocrop_autodetect, autocrop_soft, autocrop_fixedsize, autocrop_subs);
    buffer[sizeof(buffer)-1] = 0;
    return buffer;
  }
  return NULL;
}

const char *config_t::FfmpegPpOptions(void)
{
  if(ffmpeg_pp) {
    static char buffer[128];
    if(*ffmpeg_pp_mode)
      snprintf(buffer, sizeof(buffer), "quality=%d,mode=%s", ffmpeg_pp_quality, ffmpeg_pp_mode);
    else
      snprintf(buffer, sizeof(buffer), "quality=%d", ffmpeg_pp_quality);
    buffer[sizeof(buffer)-1] = 0;
    return buffer;
  }
  return NULL;
}

config_t::config_t() {
  memset(this, 0, sizeof(config_t));

  strn0cpy(local_frontend, s_frontends[FRONTEND_X11],        sizeof(local_frontend));
  strn0cpy(video_driver  , s_videoDriversX11[X11_DRIVER_XV], sizeof(video_driver));
  strn0cpy(video_port    , "0.0", sizeof(video_port));
  strn0cpy(modeline      , "",    sizeof(modeline));

  strn0cpy(audio_driver , s_audioDrivers[AUDIO_DRIVER_ALSA], sizeof(audio_driver));
  strn0cpy(audio_port   , "default", sizeof(audio_port));
  speaker_type = SPEAKERS_STEREO;

  post_plugins = NULL;

  audio_delay       = 0;
  audio_compression = 0;
  memset(audio_equalizer,0,sizeof(audio_equalizer));
  strn0cpy(audio_visualization, "goom", sizeof(audio_visualization));
  strn0cpy(audio_vis_goom_opts, "fps:25,width:720,height:576", sizeof(audio_vis_goom_opts));

  headphone = 0;
  audio_upmix = 0;
  audio_surround = 0;

  decoder_priority     = DECODER_PRIORITY_NORMAL;
  pes_buffers          = i_pesBufferSize[PES_BUFFERS_SMALL_250];
  strn0cpy(deinterlace_method, s_deinterlaceMethods[DEINTERLACE_NONE], sizeof(deinterlace_method));
  strn0cpy(deinterlace_opts, DEFAULT_DEINTERLACE_OPTS, sizeof(deinterlace_opts));
  ffmpeg_pp            = 0;
  ffmpeg_pp_quality    = 3;
  strn0cpy(ffmpeg_pp_mode, "de", sizeof(ffmpeg_pp_mode));
  display_aspect       = 0;     /* auto */

  hide_main_menu       = 0;
  prescale_osd         = 1;
  prescale_osd_downscale = 0;
  unscaled_osd         = 0;
  unscaled_osd_opaque  = 0;
  unscaled_osd_lowresvideo = 1;

  spu_autoshow = 0;
  memset(spu_lang, 0, sizeof(spu_lang));
  strn0cpy(spu_lang[0], "en", sizeof(spu_lang[0]));
  strn0cpy(spu_lang[2], "de", sizeof(spu_lang[2]));
  strn0cpy(spu_lang[1], "fi", sizeof(spu_lang[1]));
  //strn0cpy(spu_lang[3], "" , sizeof(spu_lang[3]));

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
  remote_http_files    = 1; /* allow http streaming of media files to xineliboutput clients 
			     * (currently replayed media file from xineliboutput media player) 
			     *  - will be used if client can't access file directly (nfs etc.) */

  strn0cpy(remote_rtp_addr, "224.0.1.9", sizeof(remote_rtp_addr));
  remote_rtp_port = LISTEN_PORT;
  remote_rtp_ttl = 1;
  remote_rtp_always_on = 0;
  remote_rtp_sap = 1;

  remote_use_rtsp      = 1; /* allow generic rtsp for primary device. needs enabled udp or rtp */
  remote_use_rtsp_ctrl = 0; /* allow rtsp to control primary device (play/pause/seek...) */
  remote_use_http      = 1; /* allow generic http streaming (primary device output) */
  remote_use_http_ctrl = 0; /* allow http to control primary device (play/pause/seek...) */

  remote_iface[0] = 0;   /* use only this interface - undefined -> any/all */
  remote_address[0] = 0; /* bind locally to this IP - undefined -> any/all */

  use_x_keyboard = 1;

  hue          = -1; 
  saturation   = -1; 
  contrast     = -1; 
  brightness   = -1; 
  overscan = 0;

  strn0cpy(browse_files_dir,  VideoDirectory, sizeof(browse_files_dir));
  strn0cpy(browse_music_dir,  VideoDirectory, sizeof(browse_music_dir));
  strn0cpy(browse_images_dir, VideoDirectory, sizeof(browse_images_dir));
  cache_implicit_playlists = 1;
  enable_id3_scanner = 1;

  main_menu_mode = ShowMenu;
  force_primary_device = 0;

  m_ProcessedArgs = NULL;
};

static uint8_t g_hidden_options[sizeof(config_t)] = {0};
static uint8_t g_readonly_options[sizeof(config_t)] = {0};
uint8_t *config_t::hidden_options   = &g_hidden_options[0];
uint8_t *config_t::readonly_options = &g_readonly_options[0];

bool config_t::ProcessArg(const char *Name, const char *Value)
{
  char *s = m_ProcessedArgs;
  m_ProcessedArgs = NULL;
  if(SetupParse(Name, Value)) {
    asprintf(&m_ProcessedArgs, "%s%s ", s?s:" ", Name);
    free(s);
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
     *(pt-1) == ' ' && *(pt+strlen(Name)) == ' ') {
    LOGDBG("Skipping configuration entry %s=%s (overridden in command line)", Name, Value);
    return true;
  }

       if (!strcasecmp(Name, "Frontend"))           STRN0CPY(local_frontend, Value);
  else if (!strcasecmp(Name, "Modeline"))           STRN0CPY(modeline, Value);
  else if (!strcasecmp(Name, "VideoModeSwitching")) modeswitch = atoi(Value);
  else if (!strcasecmp(Name, "Fullscreen"))         fullscreen = atoi(Value);
  else if (!strcasecmp(Name, "DisplayAspect"))      display_aspect = strstra(Value, s_aspects, 0);
  else if (!strcasecmp(Name, "ForcePrimaryDevice")) force_primary_device = atoi(Value);

  else if (!strcasecmp(Name, "X11.WindowWidth"))  width = atoi(Value);
  else if (!strcasecmp(Name, "X11.WindowHeight")) height = atoi(Value);
  else if (!strcasecmp(Name, "X11.UseKeyboard"))  use_x_keyboard = atoi(Value);

  else if (!strcasecmp(Name, "Audio.Driver")) STRN0CPY(audio_driver, Value);
  else if (!strcasecmp(Name, "Audio.Port"))   STRN0CPY(audio_port, Value);
  else if (!strcasecmp(Name, "Audio.Speakers")) speaker_type = strstra(Value, s_speakerArrangements, 
								       SPEAKERS_STEREO);
  else if (!strcasecmp(Name, "Audio.Delay"))  audio_delay = atoi(Value);
  else if (!strcasecmp(Name, "Audio.Compression")) audio_compression = atoi(Value);
  else if (!strcasecmp(Name, "Audio.Visualization.GoomOpts")) STRN0CPY(audio_vis_goom_opts, Value);
  else if (!strcasecmp(Name, "Audio.Visualization")) STRN0CPY(audio_visualization, Value);
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

  else if (!strcasecmp(Name, "OSD.SpuAutoSelect")) spu_autoshow = atoi(Value);
  else if (!strcasecmp(Name, "OSD.SpuLang0")) STRN0CPY(spu_lang[0], Value);
  else if (!strcasecmp(Name, "OSD.SpuLang1")) STRN0CPY(spu_lang[1], Value);
  else if (!strcasecmp(Name, "OSD.SpuLang2")) STRN0CPY(spu_lang[2], Value);
  else if (!strcasecmp(Name, "OSD.SpuLang3")) STRN0CPY(spu_lang[3], Value);

  else if (!strcasecmp(Name, "RemoteMode"))          remote_mode = atoi(Value);
  else if (!strcasecmp(Name, "Remote.ListenPort"))   listen_port = atoi(Value);
  else if (!strcasecmp(Name, "Remote.Keyboard"))     use_remote_keyboard = atoi(Value);
  else if (!strcasecmp(Name, "Remote.UseTcp"))       remote_usetcp = atoi(Value);
  else if (!strcasecmp(Name, "Remote.UseUdp"))       remote_useudp = atoi(Value);
  else if (!strcasecmp(Name, "Remote.UseRtp"))       remote_usertp = atoi(Value);
  else if (!strcasecmp(Name, "Remote.UsePipe"))      remote_usepipe= atoi(Value);
  else if (!strcasecmp(Name, "Remote.UseHttp"))      remote_http_files = atoi(Value);
  else if (!strcasecmp(Name, "Remote.UseBroadcast")) remote_usebcast = atoi(Value);

  else if (!strcasecmp(Name, "Remote.Rtp.Address"))  STRN0CPY(remote_rtp_addr, Value);
  else if (!strcasecmp(Name, "Remote.Rtp.Port"))     remote_rtp_port = atoi(Value);
  else if (!strcasecmp(Name, "Remote.Rtp.TTL"))      remote_rtp_ttl = atoi(Value);
  else if (!strcasecmp(Name, "Remote.Rtp.AlwaysOn")) remote_rtp_always_on = atoi(Value);
  else if (!strcasecmp(Name, "Remote.Rtp.SapAnnouncements")) remote_rtp_sap = atoi(Value);

  else if (!strcasecmp(Name, "Remote.AllowRtsp"))     remote_use_rtsp = atoi(Value);
  else if (!strcasecmp(Name, "Remote.AllowRtspCtrl")) remote_use_rtsp_ctrl = atoi(Value);
  else if (!strcasecmp(Name, "Remote.AllowHttp"))     remote_use_http = atoi(Value);
  else if (!strcasecmp(Name, "Remote.AllowHttpCtrl")) remote_use_http_ctrl = atoi(Value);

  else if (!strcasecmp(Name, "Remote.Iface"))     STRN0CPY(remote_iface,   Value);
  else if (!strcasecmp(Name, "Remote.LocalIP"))   STRN0CPY(remote_address, Value);

  else if (!strcasecmp(Name, "Decoder.Priority"))    decoder_priority=strstra(Value,s_decoderPriority,1);
  else if (!strcasecmp(Name, "Decoder.PesBuffers"))  pes_buffers=atoi(Value);

  else if (!strcasecmp(Name, "Video.Driver"))      STRN0CPY(video_driver, Value);
  else if (!strcasecmp(Name, "Video.Port"))        STRN0CPY(video_port, Value);
  else if (!strcasecmp(Name, "Video.Scale"))       scale_video = atoi(Value);
  else if (!strcasecmp(Name, "Video.DeinterlaceOptions")) STRN0CPY(deinterlace_opts, Value);
  else if (!strcasecmp(Name, "Video.Deinterlace")) STRN0CPY(deinterlace_method, Value);
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

  else if (!strcasecmp(Name, "Post.pp.Enable"))    ffmpeg_pp = atoi(Value);
  else if (!strcasecmp(Name, "Post.pp.Quality"))   ffmpeg_pp_quality = atoi(Value);
  else if (!strcasecmp(Name, "Post.pp.Mode"))      STRN0CPY(ffmpeg_pp_mode, Value);
#if 1 // 1.0.0pre6
  else if (!strcasecmp(Name, "BrowseFilesDir"))    STRN0CPY(browse_files_dir, Value);
  else if (!strcasecmp(Name, "BrowseMusicDir"))    STRN0CPY(browse_music_dir, Value);
  else if (!strcasecmp(Name, "BrowseImagesDir"))   STRN0CPY(browse_images_dir, Value);
#endif
  else if (!strcasecmp(Name, "Media.BrowseFilesDir"))    STRN0CPY(browse_files_dir, Value);
  else if (!strcasecmp(Name, "Media.BrowseMusicDir"))    STRN0CPY(browse_music_dir, Value);
  else if (!strcasecmp(Name, "Media.BrowseImagesDir"))   STRN0CPY(browse_images_dir, Value);
  else if (!strcasecmp(Name, "Media.CacheImplicitPlaylists")) cache_implicit_playlists = atoi(Value);
  else if (!strcasecmp(Name, "Media.EnableID3Scanner"))  enable_id3_scanner = atoi(Value);

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


