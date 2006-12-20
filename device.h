/*
 * device.h:
 *
 * See the main source file 'xineliboutput.c' for copyright information and
 * how to reach the author.
 *
 * $Id: device.h,v 1.17 2006-12-20 09:52:27 phintuka Exp $
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
class cFunctor;

typedef enum {
  miTitle  = 0,
  miTrack  = 1,
  miArtist = 2,
  miAlbum  = 3,
  mi_Count = 4
} eMetainfoType;

#define MAX_METAINFO_LEN 63

class cXinelibDevice : public cDevice 
{

  // Singleton

  private:
    static cXinelibDevice* m_pInstance; // singleton
    cXinelibDevice();                   //
    cXinelibDevice(cXinelibDevice&);    // no copy constructor

  public:
    static cXinelibDevice& Instance(void); // singleton
    static void Dispose(void);

    virtual ~cXinelibDevice();


  // device start/stop (from cPlugin)

  public:
    bool StartDevice(void);
    void StopDevice(void);


  // function calls waiting to be executed in VDR main thread context

  private:
    cList<cFunctor> m_MainThreadFunctors; 
    cMutex m_MainThreadLock;

  public:
    void MainThreadHook(void);


  // Primary device switching

  private:
    int m_OriginalPrimaryDevice;
    int m_ForcePrimaryDeviceCnt;

    void ForcePrimaryDeviceImpl(bool On);

  public:
    virtual void MakePrimaryDevice(bool On);
    void ForcePrimaryDevice(bool On);


  // Device capabilities

  public:

    virtual bool HasDecoder(void) const { return true; };
    virtual bool CanReplay(void) const { return true; };


  // Playback control

  private:
    ePlayMode m_PlayMode;
    int       m_TrickSpeed;
    int64_t   m_TrickSpeedPts;

  public:
    virtual bool SetPlayMode(ePlayMode PlayMode);
    ePlayMode GetPlayMode(void) const { return m_PlayMode; };

  protected:
    virtual void    Clear(void);
    virtual void    Play(void);
    virtual void    TrickSpeed(int Speed);
    virtual void    Freeze(void);
    virtual bool    Flush(int TimeoutMs = 0);
    virtual int64_t GetSTC(void);


  // Video format facilities

  public:
    virtual void SetVideoDisplayFormat(eVideoDisplayFormat VideoDisplayFormat);
    virtual void SetVideoFormat(bool VideoFormat16_9);
    virtual eVideoSystem GetVideoSystem(void);


  // Track facilities

  protected:
    virtual void SetAudioTrackDevice(eTrackType Type);

  private:
    // (DVD) SPU tracks, -> cDevice
    tTrackId m_DvdSpuTrack[64];
    int      m_CurrentDvdSpuTrack;

    char     m_MetaInfo[mi_Count][MAX_METAINFO_LEN+1];

  public:
    void ClrAvailableDvdSpuTracks(bool NotifyFrontend = true);
    bool SetAvailableDvdSpuTrack(int Type, const char *lang = NULL, bool Current = false);

    int   NumDvdSpuTracks(void) const;
    const tTrackId *GetDvdSpuTrack(int Type) const;
    const char *GetDvdSpuLang(int Type) const;

    int   GetCurrentDvdSpuTrack(void) const { return m_CurrentDvdSpuTrack; }
    bool  SetCurrentDvdSpuTrack(int Type);

    const char *GetMetaInfo(eMetainfoType Type);
    void  SetMetaInfo(eMetainfoType Type, const char *Value);

  // Audio facilities

  private:
    eTrackType m_LastTrack;
    int        m_AudioChannel;

  protected:
    virtual int  GetAudioChannelDevice(void) { return m_AudioChannel; }
    virtual void SetAudioChannelDevice(int AudioChannel);
    virtual void SetVolumeDevice(int Volume);
    virtual void SetDigitalAudioDevice(bool On);


  // Image grabbing

  public:

#if VDRVERSNUM < 10338
    virtual bool GrabImage(const char *FileName, bool Jpeg = true, 
			   int Quality = -1, int SizeX = -1, int SizeY = -1);

#else
    virtual uchar *GrabImage(int &Size, bool Jpeg = true, 
			     int Quality = -1, int SizeX = -1, int SizeY = -1);
#endif


  // SPU decoder

  private:
    cSpuDecoder *m_spuDecoder;

    friend class cXineSpuDecoder;

  public:
    virtual cSpuDecoder *GetSpuDecoder(void);


  // Messages from StatusMonitor:

  private:
    cXinelibStatusMonitor *m_statusMonitor;
    bool m_liveMode;

  public:
    void SetTvMode(cChannel *Channel);
    void SetReplayMode(void);
    void StopOutput(void);


  // Osd Commands (from cXinelibOsd)

  public:
    void OsdCmd(void *cmd);


  // Configuration

  private:
    cList<cXinelibThread>  m_clients;
    cXinelibThread        *m_server;
    cXinelibThread        *m_local;

  public:
    void ConfigureOSD(bool prescale_osd, bool unscaled_osd);
    void ConfigurePostprocessing(const char *deinterlace_method, 
				 int audio_delay,
				 int audio_compression, 
				 const int *audio_equalizer,
				 int audio_surround, 
				 int speaker_type);
    void ConfigurePostprocessing(const char *name, bool on=true, 
				 const char *args=NULL);
    void ConfigureVideo(int hue, int saturation, int brightness, int contrast,
			int overscan);
    // local mode:
    void ConfigureWindow(int fullscreen, int width, int height, 
			 int modeswitch, const char *modeline, 
			 int aspect, int scale_video, int field_order);
    void ConfigureDecoder(int pes_buffers, int priority);
    // remote mode:
    void Listen(bool activate, int port);


  // File playback

  private:
    bool m_PlayingFile;

  public:
    bool PlayFile(const char *Filename, int Position=0, bool LoopPlay=false);
    int  PlayFileCtrl(const char *Cmd);
    bool EndOfStreamReached(void);


  // Stream data

  private:
    bool m_ac3Present;
    bool m_spuPresent;
    bool m_RadioStream;
    int  m_AudioCount;
    bool m_SkipAudio;
    bool m_StreamStart;
    bool m_Polled;

    int PlayAny(const uchar *Data, int Length);

  protected:

    virtual bool Poll(cPoller &Poller, int TimeoutMs = 0);

    virtual void StillPicture(const uchar *Data, int Length);

    virtual int  PlayVideo(const uchar *Data, int Length);
    virtual int  PlayAudio(const uchar *Data, int Length, uchar Id);

    virtual int  PlaySpu(const uchar *Data, int Length, uchar Id);

    // override cDevice to get DVD SPUs
    virtual int PlayPesPacket(const uchar *Data, int Length,
			      bool VideoOnly = false);

#if VDRVERSNUM < 10342
    // API changed in VDR 1.3.42
    virtual int  PlayAudio(const uchar *Data, int Length)
    { return PlayAudio(Data, Length, 0); }
#endif
};

#endif // __XINELIB_DEVICE_H
