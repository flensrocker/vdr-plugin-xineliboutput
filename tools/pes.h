/*
 * pes.h: PES header definitions
 *
 * See the main source file 'xineliboutput.c' for copyright information and
 * how to reach the author.
 *
 * $Id: pes.h,v 1.5 2008-01-24 08:33:59 phintuka Exp $
 *
 */

#ifndef _PES_H_
#define _PES_H_

//
// Constants
//

#define PES_CHUNK_SIZE 2048

#define MAX_SCR ((int64_t)0x1ffffffffLL)

// PES PIDs
#define PRIVATE_STREAM1   0xBD
#define PADDING_STREAM    0xBE
#define PRIVATE_STREAM2   0xBF
#define AUDIO_STREAM_S    0xC0      /* 1100 0000 */
#define AUDIO_STREAM_E    0xDF      /* 1101 1111 */
#define VIDEO_STREAM_S    0xE0      /* 1110 0000 */
#define VIDEO_STREAM_E    0xEF      /* 1110 1111 */

#define AUDIO_STREAM_MASK 0x1F  /* 0001 1111 */
#define VIDEO_STREAM_MASK 0x0F  /* 0000 1111 */
#define AUDIO_STREAM      0xC0  /* 1100 0000 */
#define VIDEO_STREAM      0xE0  /* 1110 0000 */

#define ECM_STREAM        0xF0
#define EMM_STREAM        0xF1
#define DSM_CC_STREAM     0xF2
#define ISO13522_STREAM   0xF3
#define PROG_STREAM_DIR   0xFF

// "picture header"
#define SC_PICTURE        0x00  

// Picture types
#define NO_PICTURE  0
#define I_FRAME     1
#define P_FRAME     2
#define B_FRAME     3

//
// Extract PTS from PES packet
//

static inline int64_t pes_extract_pts(const uchar *Data, int Length, 
				  bool& Audio, bool& Video) 
{
  /* assume mpeg2 pes header ... */
  Audio = Video = false;

  if((VIDEO_STREAM == (Data[3] & ~VIDEO_STREAM_MASK) && (Video=true)) ||
     (AUDIO_STREAM == (Data[3] & ~AUDIO_STREAM_MASK) && (Audio=true)) ||
     (PRIVATE_STREAM1 == Data[3] && (Audio=true))) {
      
    if ((Data[6] & 0xC0) != 0x80)
      return -1;
    if ((Data[6] & 0x30) != 0)
      return -1;
      
    if((Length > 14) && (Data[7] & 0x80)) { /* pts avail */
      int64_t pts;
      pts  = ((int64_t)(Data[ 9] & 0x0E)) << 29 ;
      pts |= ((int64_t) Data[10])         << 22 ;
      pts |= ((int64_t)(Data[11] & 0xFE)) << 14 ;
      pts |= ((int64_t) Data[12])         <<  7 ;
      pts |= ((int64_t)(Data[13] & 0xFE)) >>  1 ;
      return pts;
    }
  }
  return -1ULL;
}

//
// Change PTS of PES packet
//

static inline void pes_change_pts(uchar *Data, int Length, int64_t pts)
{
  /* assume mpeg2 pes header ... Assume header already HAS pts */
  if((VIDEO_STREAM == (Data[3] & ~VIDEO_STREAM_MASK)) ||
     (AUDIO_STREAM == (Data[3] & ~AUDIO_STREAM_MASK)) ||
     (PRIVATE_STREAM1 == Data[3])) {
      
    if ((Data[6] & 0xC0) != 0x80)
      return;
    if ((Data[6] & 0x30) != 0)
      return;
      
    if((Length > 14) && (Data[7] & 0x80)) { /* pts avail */
      Data[ 9] = ((pts >> 29) & 0x0E) | (Data[ 9] & 0xf1);
      Data[10] = ((pts >> 22) & 0xFF);
      Data[11] = ((pts >> 14) & 0xFE) | (Data[11] & 0x01);
      Data[12] = ((pts >> 7 ) & 0xFF);
      Data[13] = ((pts << 1 ) & 0xFE) | (Data[13] & 0x01);
    }
  }
}

//
// Remove pts from PES packet (set to 0LL)
//

static inline void pes_strip_pts(uchar *Data, int Len)
{
  if(VIDEO_STREAM == (Data[3] & ~VIDEO_STREAM_MASK) ||
     AUDIO_STREAM == (Data[3] & ~AUDIO_STREAM_MASK) ||
     PRIVATE_STREAM1 == Data[3]) {

    // MPEG1 PES 
    if ((Data[6] & 0xC0) != 0x80) { 
      Data += 6;
      Len -= 6;

      // skip stuffing 
      while ((Data[0] & 0x80) == 0x80) { 
	Data++;
        Len--;
      }
      if ((Data[0] & 0xc0) == 0x40) {
        // STD_buffer_scale, STD_buffer_size 
        Data += 2;
        Len -= 2;
      }
      
      if(Len<5) return;
      if ((Data[0] & 0xf0) == 0x20) { 
	// zero PTS
        Data[0] &= ~0x0E;
        Data[1] = 0;
        Data[2] &= ~0xFE;
        Data[3] = 0;
        Data[4] &= ~0xFE;
        return;
      }
      if(Len<10) return;
      if ((Data[0] & 0xf0) == 0x30) { 
	// zero PTS & DTS
//((uint32*)Data)[0] &= 0x0E00FE00;
        Data[0] &= ~0x0E;
        Data[1] = 0;
        Data[2] &= ~0xFE;
        Data[3] = 0;
//((uint32*)Data)[1] &= 0xFE0E00FE;
        Data[4] &= ~0xFE;
        Data[5] &= ~0x0E;
        Data[6] = 0;
        Data[7] &= ~0xFE;
//((uint32*)Data)[2] &= 0x00FEFFFF;
        Data[8] = 0;
        Data[9] &= ~0xFE;
        return;
      }

    // MPEG2 PES
    } else {
      if ((Data[6] & 0xC0) != 0x80)
        return;
      if ((Data[6] & 0x30) != 0)
        return;

      if(Len<14) return;
      if (Data[7] & 0x80) { 
	// PTS
        if(Data[8]<5) return;
        Data[ 9] &= ~0x0E;
        Data[10] = 0;
        Data[11] &= ~0xFE;
        Data[12] = 0;
        Data[13] &= ~0xFE;
      }
      if(Len<19) return;
      if (Data[7] & 0x40) {
	// DTS
        if(Data[8]<10) return;
        Data[14] &= ~0x0E;
        Data[15] = 0;
        Data[16] &= ~0xFE;
        Data[17] = 0;
        Data[18] &= ~0xFE;
      }
    }
  }
}

//
// Extract PES packet length
//

static inline int pes_packet_len(const uchar *header, const int maxlen, bool &isMpeg1)
{
  if(VIDEO_STREAM == (header[3] & ~VIDEO_STREAM_MASK) ||
     AUDIO_STREAM == (header[3] & ~AUDIO_STREAM_MASK) ||
     PRIVATE_STREAM1 == header[3]) {
    isMpeg1 = ((header[6] & 0xC0) != 0x80);
    return 6 + (header[4] << 8 | header[5]);
  } else if (header[3] == PADDING_STREAM) {
    isMpeg1 = false;
    return (6 + (header[4] << 8 | header[5]));
  } else if (header[3] == 0xBA) {
    if ((header[4] & 0x40) == 0) { /* mpeg1 */
      isMpeg1 = true;
      return 12;
    } else { /* mpeg 2 */
      isMpeg1 = false;
      return 14 + (header[0xD] & 0x07);
    }
  } else if (header[3] <= 0xB9) {
    int len=3;
    return -3;
    isMpeg1 = false;
    while(len+2<maxlen) {
      if(!header[len] && !header[len+1] && header[len+2] == 1)
        return len;
      len++;
    }
    return -len;
  }
  isMpeg1 = false;
  return -(6 + (header[4] << 8 | header[5]));
}

//
// Extract video frame size from video PES packet
//  - returns 1 if size was found
//

static inline int GetVideoSize(const uchar *buf, int length, int *width, int *height)
{
  int i = 8;         // the minimum length of the video packet header
  i += buf[i] + 1;   // possible additional header bytes
  for (; i < length-6; i++) {
    if (buf[i] == 0 && buf[i + 1] == 0 && buf[i + 2] == 1) {
      if(buf[i + 3] == 0xb3) {
	int d = (buf[i+4] << 16) | (buf[i+5] << 8) | buf[i+6];
	*width = (d >> 12);
	*height = (d & 0xfff);
	return 1;
      }
    }
  }
  return 0;
}

static inline bool IsFrameH264(const uchar *Data, int Count)
{
  if (Count < 9 || Count < 9 + Data[8])
    return false;
  if ( (Data[6] & 0xC0) != 0x80)  /* MPEG 2 */
    return false;

  Data += 9 + Data[8];

  if (!Data[0] && !Data[1] && Data[2] == 0x01 && Data[3] == 0x09)
    return true;
  return false;
}

// from vdr/remux.c:
static inline int ScanVideoPacket(const uchar *Data, int Count, /*int Offset,*/ 
				  uchar &PictureType)
{
  // Scans the video packet starting at Offset and returns its length.
  // If the return value is -1 the packet was not completely in the buffer.
  int Length = Count;
  if (Length > 0 && Length <= Count) {
    int i = 8;          // the minimum length of the video packet header
    i += Data[i] + 1;   // possible additional header bytes
    for (; i < Length-5; i++) {
      if (Data[i] == 0 && Data[i + 1] == 0 && Data[i + 2] == 1) {
	switch (Data[i + 3]) {
	  case SC_PICTURE: 
	    PictureType = (Data[i + 5] >> 3) & 0x07;
	    return Length;
	}
      }
    }
    PictureType = NO_PICTURE;
    return Length;
  }
  return -1;
}

static inline const char *PictureTypeStr(int Type)
{
  switch(Type) {
    case I_FRAME: return "I-Frame"; break;
    case B_FRAME: return "B-Frame"; break;
    case P_FRAME: return "P-Frame"; break;
    case NO_PICTURE: return "(none)"; break;
    default: break;
  }
  return "UNKNOWN";
}

#endif
