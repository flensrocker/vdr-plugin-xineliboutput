/*
 * device.h:
 *
 * See the main source file 'xineliboutput.c' for copyright information and
 * how to reach the author.
 *
 * $Id: device.h,v 1.6 2006-08-16 13:18:42 phintuka Exp $
 *
 */

#ifndef __XINELIB_DEVICE_H
#define __XINELIB_DEVICE_H

#include <vdr/config.h>
#include <vdr/device.h>
#include <vdr/tools.h>

class cXinelibStatusMonitor;
class cXinelibThread;
class cChannel;

class cXinelibDevice : public cDevice 
{
  public:
    static cXinelibDevice& Instance(void); // singleton
    static void Dispose(void);

  private:
    static cXinelibDevice* m_pInstance; // singleton
    cXinelibDevice();                   //
    cXinelibDevice(cXinelibDevice&);    // no copy constructor

  public:
    virtual ~cXinelibDevice();

    virtual bool HasDecoder(void) const { return true; };
    virtual bool CanReplay(void) const { return true; };

    virtual void MakePrimaryDevice(bool On);
    void ForcePrimaryDevice(bool On);

    virtual void Clear(void);
    virtual void Play(void);
    virtual void TrickSpeed(int Speed);
    virtual void Freeze(void);

    virtual void StillPicture(const uchar *Data, int Length);
    virtual bool Poll(cPoller &Poller, int TimeoutMs = 0);
    virtual bool Flush(int TimeoutMs = 0);
    virtual int64_t GetSTC(void);

    // Video format facilities
  public:
    virtual void SetVideoDisplayFormat(eVideoDisplayFormat VideoDisplayFormat);
    virtual void SetVideoFormat(bool VideoFormat16_9);
    virtual eVideoSystem GetVideoSystem(void);

    // Track facilities
  protected:
    virtual void SetAudioTrackDevice(eTrackType Type);

    // Audio facilities
  private:
    eTrackType m_LastTrack;
    int        m_AudioChannel;
  protected:
    virtual int  GetAudioChannelDevice(void) {return m_AudioChannel;}
    virtual void SetAudioChannelDevice(int AudioChannel);
    virtual void SetVolumeDevice(int Volume);
    virtual void SetDigitalAudioDevice(bool On);
  public:

#if VDRVERSNUM < 10338
    virtual bool GrabImage(const char *FileName, bool Jpeg = true, 
			   int Quality = -1, int SizeX = -1, int SizeY = -1);

#else
    virtual uchar *GrabImage(int &Size, bool Jpeg = true, 
			     int Quality = -1, int SizeX = -1, int SizeY = -1);
#endif

    virtual cSpuDecoder *GetSpuDecoder(void);

    // Messages from StatusMonitor:
    void SetTvMode(cChannel *Channel);
    void SetReplayMode(void);
    void StopOutput(void);

    // device startup (from cPlugin)
    bool StartDevice(void);
    void StopDevice(void);

    // Osd Commands (from cXinelibOsd)
    void OsdCmd(void *cmd);

    // Configuration
    void ConfigureOSD(bool prescale_osd, bool unscaled_osd);
    void ConfigurePostprocessing(const char *deinterlace_method, 
				 int audio_delay,
				 int audio_compression, 
				 const int *audio_equalizer,
				 int audio_surround);
    void ConfigurePostprocessing(const char *name, bool on=true, 
				 const char *args=NULL);
    void ConfigureVideo(int hue, int saturation, int brightness, int contrast);
    // local mode:
    void ConfigureWindow(int fullscreen, int width, int height, 
			 int modeswitch, const char *modeline, 
			 int aspect, int scale_video, int field_order);
    void ConfigureDecoder(int pes_buffers, int priority);
    // remote mode:
    void Listen(bool activate, int port);

    // File playback
    bool PlayFile(const char *Filename, int Position=0, bool LoopPlay=false);
    int  PlayFileCtrl(const char *Cmd);
    bool EndOfStreamReached(void);

  private:

    int PlayAny(const uchar *Data, int Length);

  protected:
    virtual int  PlayVideo(const uchar *Data, int Length);
    virtual int  PlayAudio(const uchar *Data, int Length, uchar Id);
#if VDRVERSNUM< 10342
    virtual int  PlayAudio(const uchar *Data, int Length)
    { return PlayAudio(Data, Length, 0); }
#endif
    virtual int  PlaySpu(const uchar *Data, int Length, uchar Id);

#if 1
    // override cDevice to get DVD SPUs
    virtual int PlayPesPacket(const uchar *Data, int Length,
			      bool VideoOnly = false);

    // -> cDevice
    int  m_DvdSpuTracks;
    int  m_CurrentDvdSpuTrack;
    bool m_DvdSpuTrack[64];
    void ClrAvailableDvdSpuTracks(void);
    bool SetAvailableDvdSpuTrack(int Type);

  public:
    int  NumDvdSpuTracks(void) const { return m_DvdSpuTracks; }
    int  GetCurrentDvdSpuTrack(void) { return m_CurrentDvdSpuTrack; }
    bool SetCurrentDvdSpuTrack(int Type);
    bool HasDvdSpuTrack(int Type) const;
#endif

    virtual bool SetPlayMode(ePlayMode PlayMode);
    ePlayMode GetPlayMode(void) const { return playMode; };

  protected:
    ePlayMode playMode;

    cList<cXinelibThread> m_clients;
    cXinelibThread        *m_server;
    cXinelibThread        *m_local;
    cXinelibStatusMonitor *m_statusMonitor;
    cSpuDecoder           *m_spuDecoder;

    bool m_ac3Present;
    bool m_spuPresent;

    bool m_liveMode;
    int  m_TrickSpeed;
    int64_t m_TrickSpeedPts;
    bool m_SkipAudio;
    bool m_PlayingFile;
    bool m_StreamStart;
    bool m_RadioStream;
    int  m_AudioCount;

    friend class cXineSpuDecoder;
};

#endif // __XINELIB_DEVICE_H
