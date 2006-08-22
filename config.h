/*
 * config.h: Global configuration and user settings
 *
 * See the main source file 'xineliboutput.c' for copyright information and
 * how to reach the author.
 *
 * $Id: config.h,v 1.6 2006-08-22 03:45:34 phintuka Exp $
 *
 */

#ifndef _XINELIB_CONFIG_H_
#define _XINELIB_CONFIG_H_

#include <string.h>

// Decoder buffer size
#define PES_BUFFERS_CUSTOM      0
#define PES_BUFFERS_TINY_50     1
#define PES_BUFFERS_SMALL_250   2
#define PES_BUFFERS_MEDIUM_500  3
#define PES_BUFFERS_LARGE_1000  4
#define PES_BUFFERS_HUGE_2000   5
#define PES_BUFFERS_count       6

// Output window aspect ratio
#define ASPECT_AUTO     0
#define ASPECT_DEFAULT  1
#define ASPECT_4_3      2
#define ASPECT_16_9     3
#define ASPECT_16_10    4
#define ASPECT_PAN_SCAN 5
#define ASPECT_CENTER_CUT_OUT 6
#define ASPECT_count          7

// De-interlace method
#define DEINTERLACE_NONE         0
#define DEINTERLACE_BOB          1
#define DEINTERLACE_WEAVE        2
#define DEINTERLACE_GREEDY       3
#define DEINTERLACE_ONEFIELD     4
#define DEINTERLACE_ONEFIELD_XV  5
#define DEINTERLACE_LINEARLEND   6
#define DEINTERLACE_TVTIME       7
#define DEINTERLACE_count        8

// Decoder priority
#define DECODER_PRIORITY_LOW     0
#define DECODER_PRIORITY_NORMAL  1
#define DECODER_PRIORITY_HIGH    2
#define DECODER_PRIORITY_count   3

#define FIELD_ORDER_NORMAL       0
#define FIELD_ORDER_INVERTED     1
#define FIELD_ORDER_count        2

// Audio driver
#define AUDIO_DRIVER_AUTO        0
#define AUDIO_DRIVER_ALSA        1
#define AUDIO_DRIVER_OSS         2
#define AUDIO_DRIVER_NONE        3
#define AUDIO_DRIVER_ARTS        4
#define AUDIO_DRIVER_ESOUND      5
#define AUDIO_DRIVER_count       6

// Video driver
#define X11_DRIVER_AUTO          0
#define X11_DRIVER_XSHM          1
#define X11_DRIVER_XV            2
#define X11_DRIVER_XVMC          3
#define X11_DRIVER_XXMC          4
#define X11_DRIVER_NONE          5
#define X11_DRIVER_count         6

#define FB_DRIVER_AUTO           0
#define FB_DRIVER_FB             1
#define FB_DRIVER_DIRECTFB       2
#define FB_DRIVER_NONE           3
#define FB_DRIVER_count          4

// Local frontend
#define FRONTEND_X11             0
#define FRONTEND_FB              1
#define FRONTEND_NONE            2
#define FRONTEND_count           3
#define DEFAULT_FRONTEND         "sxfe"

#define LISTEN_PORT       37890
#define LISTEN_PORT_S    "37890"
#define DISCOVERY_PORT    37890

#define AUDIO_EQ_30HZ    0
#define AUDIO_EQ_60HZ    1
#define AUDIO_EQ_125HZ   2
#define AUDIO_EQ_250HZ   3
#define AUDIO_EQ_500HZ   4
#define AUDIO_EQ_1000HZ  5
#define AUDIO_EQ_2000HZ  6
#define AUDIO_EQ_4000HZ  7 
#define AUDIO_EQ_8000HZ  8
#define AUDIO_EQ_16000HZ 9
#define AUDIO_EQ_count   10

#define AUDIO_VIS_NONE   0
#define AUDIO_VIS_GOOM   1
#define AUDIO_VIS_count  5

/* speaker arrangements: xine, audio_out_alsa.c */
#define SPEAKERS_MONO          0
#define SPEAKERS_STEREO        1
#define SPEAKERS_HEADPHONES    2
#define SPEAKERS_SURROUND21    3
#define SPEAKERS_SURROUND3     4 
#define SPEAKERS_SURROUND4     5
#define SPEAKERS_SURROUND41    6
#define SPEAKERS_SURROUND5     7
#define SPEAKERS_SURROUND51    8
#define SPEAKERS_SURROUND6     9
#define SPEAKERS_SURROUND61    10
#define SPEAKERS_SURROUND71    11
#define SPEAKERS_A52_PASSTHRU  12 
#define SPEAKERS_count         13 

typedef enum {
  ShowMenu   = 0,
  ShowEq     = 1,
  ShowFiles  = 2,
  ShowMusic  = 3,
  ShowImages = 4,
  CloseOsd   = 5
} eMainMenuMode;

class config_t {
  public:
    static const char *s_bufferSize[];
    static const int   i_pesBufferSize[];
    static const char *s_aspects[];
    static const char *s_deinterlaceMethods[];
    static const char *s_deinterlaceMethodNames[];
    static const char *s_decoderPriority[];
    static const char *s_fieldOrder[];
    static const char *s_audioDriverNames[];
    static const char *s_audioDrivers[];
    static const char *s_videoDriverNamesX11[];
    static const char *s_videoDriversX11[];
    static const char *s_videoDriverNamesFB[];
    static const char *s_videoDriversFB[];
    static const char *s_frontendNames[];
    static const char *s_frontends[];
    static const char *s_frontend_files[];
    static const char *s_audioEqNames[];
    static const char *s_audioVisualizations[];
    static const char *s_audioVisualizationNames[];
    static const char *s_speakerArrangements[];

  public:
    char video_driver[32];
    char video_port[64];     // X11: DISPLAY=...
    char audio_driver[32];
    char audio_port[32];
    int  speaker_type;
    char *post_plugins;      // from command line options

    int  audio_delay;        // in ms
    int  audio_compression;  // 100%(=off)...500%
    int  audio_equalizer[AUDIO_EQ_count];
    char audio_visualization[256];
    //char audio_vis_goom_opts[256];
    int  audio_surround;
    int  headphone;
    int  audio_upmix;
    
    int  decoder_priority;
    int  pes_buffers;
    char deinterlace_method[32];
    char deinterlace_opts[256];
    int  display_aspect;
    
    int  hide_main_menu;
    int  prescale_osd;
    int  prescale_osd_downscale;
    int  unscaled_osd;
    int  unscaled_osd_opaque;
    int  unscaled_osd_lowresvideo;
    int  alpha_correction;
    int  alpha_correction_abs;

    char local_frontend[256];
    char modeline[128];
    
    int  fullscreen;
    int  modeswitch;
    int  width;
    int  height;
    int  scale_video;
    int  field_order;
    int  autocrop;
    int  autocrop_autodetect;
    int  autocrop_soft;
    int  autocrop_fixedsize;
    int  autocrop_subs;
    int  exit_on_close;
    
    int  remote_mode;
    int  listen_port;
    int  use_remote_keyboard;
    int  remote_usetcp, remote_useudp, remote_usertp, remote_usepipe;
    int  remote_usebcast;

    char remote_rtp_addr[64];
    int  remote_rtp_port;
    int  remote_rtp_ttl;
    int  remote_rtp_always_on;

    int  use_x_keyboard;

    int  hue;                 // 0...0xffff, -1 == off
    int  saturation;          // 0...0xffff, -1 == off
    int  contrast;            // 0...0xffff, -1 == off
    int  brightness;          // 0...0xffff, -1 == off
    
    char browse_files_dir[4096];
    char browse_music_dir[4096];
    char browse_images_dir[4096];
    
    eMainMenuMode main_menu_mode;
    int  force_primary_device;

    config_t();
    bool SetupParse(const char *Name, const char *Value);
    bool ProcessArgs(int argc, char *argv[]);

    bool IsImageFile(const char *);
    bool IsVideoFile(const char *);
    const char *AutocropOptions(void);

  protected:
    bool ProcessArg(const char *Name, const char *Value);
    char *m_ProcessedArgs;
};

// Global instance
extern config_t xc;

// Find index of string in array of strings
static inline int strstra(const char *str, const char *stra[], int def_index)
{
  int i;
  for(i=0; stra[i]; i++)
    if(!strcmp(str,stra[i]))
      return i;
  return def_index;
}

#endif //_XINELIB_CONFIG_H_

