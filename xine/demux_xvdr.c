/*
 * Copyright (C) 2000-2006 the xine project
 *
 * This file is part of xine, a free video player.
 *
 * xine is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * xine is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110, USA
 *
 * demultiplexer for mpeg 1/2 program streams
 * used with fixed blocksize devices (like dvd/vcd)
 */

#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

#define LOG_MODULE "demux_mpeg_block"

#include <xine/xine_internal.h>
#include <xine/xineutils.h>
#include <xine/demux.h>

#define DISC_TRESHOLD       90000

#define WRAP_THRESHOLD     120000
#define PTS_AUDIO 0
#define PTS_VIDEO 1


/* redefine abs as macro to handle 64-bit diffs.
   i guess llabs may not be available everywhere */
#define abs(x) ( ((x)<0) ? -(x) : (x) )

typedef struct demux_mpeg_block_s {
  demux_plugin_t        demux_plugin;

  xine_stream_t        *stream; 
  fifo_buffer_t        *audio_fifo;
  fifo_buffer_t        *video_fifo;

  input_plugin_t       *input;

  int                   status;

  char                  cur_mrl[256];

  int64_t               last_pts[2];
  int                   send_newpts;
  int                   buf_flag_seek;
  uint32_t              packet_len;
  int64_t               pts;
  int64_t               dts;
  uint32_t              stream_id;
  int32_t               mpeg1;

} demux_mpeg_block_t ;

typedef struct {

  demux_class_t     demux_class;

  /* class-wide, global variables here */

  xine_t           *xine;
  config_values_t  *config;
} demux_mpeg_block_class_t;

static int32_t parse_video_stream(demux_mpeg_block_t *this, uint8_t *p, buf_element_t *buf);
static int32_t parse_audio_stream(demux_mpeg_block_t *this, uint8_t *p, buf_element_t *buf);
static int32_t parse_private_stream_1(demux_mpeg_block_t *this, uint8_t *p, buf_element_t *buf);
static int32_t parse_padding_stream(demux_mpeg_block_t *this, uint8_t *p, buf_element_t *buf);


static void check_newpts( demux_mpeg_block_t *this, int64_t pts, int video )
{
  int64_t diff;

  diff = pts - this->last_pts[video];

  if( pts && (this->send_newpts || (this->last_pts[video] && abs(diff)>WRAP_THRESHOLD) ) ) {

      if (this->buf_flag_seek) {
        _x_demux_control_newpts(this->stream, pts, BUF_FLAG_SEEK);
        this->buf_flag_seek = 0;
      } else {
        _x_demux_control_newpts(this->stream, pts, 0);
      }
      this->send_newpts = 0;

    this->last_pts[1-video] = 0;
  }

  if( pts )
    this->last_pts[video] = pts;
}

static void demux_mpeg_block_parse_pack (demux_mpeg_block_t *this) {

  buf_element_t *buf = NULL;
  uint8_t       *p;
  int32_t        result;

  this->scr = 0;

  lprintf ("read_block\n");

  buf = this->input->read_block (this->input, this->video_fifo, this->blocksize);

  if (buf==NULL) {
    this->status = DEMUX_FINISHED;
    return ;
  }

  /* If this is not a block for the demuxer, pass it
   * straight through. */
  if (buf->type != BUF_DEMUX_BLOCK) {
    buf_element_t *cbuf;

    this->video_fifo->put (this->video_fifo, buf);

    /* duplicate goes to audio fifo */

    if (this->audio_fifo) {
      cbuf = this->audio_fifo->buffer_pool_alloc (this->audio_fifo);

      cbuf->type = buf->type;
      cbuf->decoder_flags = buf->decoder_flags;
      memcpy( cbuf->decoder_info, buf->decoder_info, sizeof(cbuf->decoder_info) );
      memcpy( cbuf->decoder_info_ptr, buf->decoder_info_ptr, sizeof(cbuf->decoder_info_ptr) );

      this->audio_fifo->put (this->audio_fifo, cbuf);
    }

    lprintf ("type %08x != BUF_DEMUX_BLOCK\n", buf->type);

    return;
  }

  p = buf->content; /* len = this->blocksize; */
  buf->decoder_flags = 0;

  while(p < (buf->content + this->blocksize)) {
    if (p[0] || p[1] || (p[2] != 1)) {
      xprintf (this->stream->xine, XINE_VERBOSITY_DEBUG,
	       "demux_mpeg_block: error! %02x %02x %02x (should be 0x000001)\n", p[0], p[1], p[2]);
      xprintf (this->stream->xine, XINE_VERBOSITY_DEBUG, "demux_mpeg_block: bad block. skipping.\n");
      /* FIXME: We should find some way for the input plugin to inform us of: -
       * 1) Normal sector read.
       * 2) Sector error read due to bad crc etc.
       * Because we would like to handle these two cases differently.
       */
      buf->free_buffer (buf);
      return;
    }

    this->stream_id  = p[3];

    if (this->stream_id == 0xBD) {
      result = parse_private_stream_1(this, p, buf);
    } else if (this->stream_id == 0xBE) {
      result = parse_padding_stream(this, p, buf);
    } else if ((this->stream_id >= 0xC0)
            && (this->stream_id < 0xDF)) {
      result = parse_audio_stream(this, p, buf);
    } else if ((this->stream_id >= 0xE0)
            && (this->stream_id < 0xEF)) {
      result = parse_video_stream(this, p, buf);
    } else {
      xprintf(this->stream->xine, XINE_VERBOSITY_LOG,
	      _("xine-lib:demux_mpeg_block: "
		"Unrecognised stream_id 0x%02x. Please report this to xine developers.\n"), this->stream_id);
      buf->free_buffer (buf);
      return;
    }
    if (result < 0) {
      return;
    }
    p+=result;
  }
  xprintf (this->stream->xine, XINE_VERBOSITY_LOG,
	   _("demux_mpeg_block: error! freeing. Please report this to xine developers.\n"));
  buf->free_buffer (buf);
  return ;
}

static int32_t parse_padding_stream(demux_mpeg_block_t *this, uint8_t *p, buf_element_t *buf) {
  /* Just skip padding. */
  buf->free_buffer (buf);
  return -1;
}

/* FIXME: Extension data is not parsed, and is also not skipped. */

static int32_t parse_pes_for_pts(demux_mpeg_block_t *this, uint8_t *p, buf_element_t *buf) {
  int32_t header_len;

  this->packet_len = p[4] << 8 | p[5];

  if (this->mpeg1) {
    header_len = 6;
    p   += 6; /* packet_len -= 6; */

    while ((p[0] & 0x80) == 0x80) {
      p++;
      header_len++;
      this->packet_len--;
      /* printf ("stuffing\n");*/
    }

    if ((p[0] & 0xc0) == 0x40) {
      /* STD_buffer_scale, STD_buffer_size */
      p += 2;
      header_len += 2;
      this->packet_len -= 2;
    }

    this->pts = 0;
    this->dts = 0;

    if ((p[0] & 0xf0) == 0x20) {
      this->pts  = (int64_t)(p[ 0] & 0x0E) << 29 ;
      this->pts |=  p[ 1]         << 22 ;
      this->pts |= (p[ 2] & 0xFE) << 14 ;
      this->pts |=  p[ 3]         <<  7 ;
      this->pts |= (p[ 4] & 0xFE) >>  1 ;
      p   += 5;
      header_len+= 5;
      this->packet_len -=5;
      return header_len;
    } else if ((p[0] & 0xf0) == 0x30) {
      this->pts  = (int64_t)(p[ 0] & 0x0E) << 29 ;
      this->pts |=  p[ 1]         << 22 ;
      this->pts |= (p[ 2] & 0xFE) << 14 ;
      this->pts |=  p[ 3]         <<  7 ;
      this->pts |= (p[ 4] & 0xFE) >>  1 ;

      this->dts  = (int64_t)(p[ 5] & 0x0E) << 29 ;
      this->dts |=  p[ 6]         << 22 ;
      this->dts |= (p[ 7] & 0xFE) << 14 ;
      this->dts |=  p[ 8]         <<  7 ;
      this->dts |= (p[ 9] & 0xFE) >>  1 ;

      p   += 10;
      header_len += 10;
      this->packet_len -= 10;
      return header_len;
    } else {
      p++;
      header_len++;
      this->packet_len--;
      return header_len;
    }

  } else { /* mpeg 2 */


    if ((p[6] & 0xC0) != 0x80) {
      xine_log (this->stream->xine, XINE_LOG_MSG,
		_("demux_mpeg_block: warning: PES header reserved 10 bits not found\n"));
      buf->free_buffer(buf);
      return -1;
    }


    /* check PES scrambling_control */

    if ((p[6] & 0x30) != 0) {
      xprintf(this->stream->xine, XINE_VERBOSITY_LOG,
	      _("demux_mpeg_block: warning: PES header indicates that this stream "
		"may be encrypted (encryption mode %d)\n"), (p[6] & 0x30) >> 4);
      _x_message (this->stream, XINE_MSG_ENCRYPTED_SOURCE,
		  "Media stream scrambled/encrypted", NULL);
      this->status = DEMUX_FINISHED;
      buf->free_buffer(buf);
      return -1;
    }

    if (p[7] & 0x80) { /* pts avail */

      this->pts  = (int64_t)(p[ 9] & 0x0E) << 29 ;
      this->pts |=  p[10]         << 22 ;
      this->pts |= (p[11] & 0xFE) << 14 ;
      this->pts |=  p[12]         <<  7 ;
      this->pts |= (p[13] & 0xFE) >>  1 ;

      lprintf ("pts = %"PRId64"\n", this->pts);

    } else
      this->pts = 0;

    if (p[7] & 0x40) { /* dts avail */

      this->dts  = (int64_t)(p[14] & 0x0E) << 29 ;
      this->dts |=  p[15]         << 22 ;
      this->dts |= (p[16] & 0xFE) << 14 ;
      this->dts |=  p[17]         <<  7 ;
      this->dts |= (p[18] & 0xFE) >>  1 ;

    } else
      this->dts = 0;


    header_len = p[8];

    this->packet_len -= header_len + 3;
    return header_len + 9;
  }
  return 0;
}

static int32_t parse_private_stream_1(demux_mpeg_block_t *this, uint8_t *p, buf_element_t *buf) {

    int track, spu_id;
    int32_t result;

    result = parse_pes_for_pts(this, p, buf);
    if (result < 0) return -1;

    p += result;
    /* printf("demux_mpeg_block: private_stream_1: p[0] = 0x%02X\n", p[0]); */

    if((p[0] & 0xE0) == 0x20) {
      spu_id = (p[0] & 0x1f);

      buf->content   = p+1;
      buf->size      = this->packet_len-1;

      buf->type      = BUF_SPU_DVD + spu_id;
      buf->decoder_flags |= BUF_FLAG_SPECIAL;
      buf->decoder_info[1] = BUF_SPECIAL_SPU_DVD_SUBTYPE;
      buf->decoder_info[2] = SPU_DVD_SUBTYPE_PACKAGE;
      buf->pts       = this->pts;

      this->video_fifo->put (this->video_fifo, buf);
      lprintf ("SPU PACK put on fifo\n");

      return -1;
    }

    if ((p[0]&0xF0) == 0x80) {

      track = p[0] & 0x0F; /* hack : ac3 track */

      /* Number of frame headers
       *
       * Describes the number of a52 frame headers that start in this pack (One pack per DVD sector).
       *
       * Likely values: 1 or 2.
       */
      buf->decoder_info[1] = p[1];
      /* First access unit pointer.
       *
       * Describes the byte offset within this pack to the beginning of the first A52 frame header.
       * The PTS from this pack applies to the beginning of the first A52 frame that starts in this pack.
       * Any bytes before this offset should be considered to belong to the previous A52 frame.
       * and therefore be tagged with a PTS value derived from the previous pack.
       *
       * Likely values: Anything from 1 to the size of an A52 frame.
       */
      buf->decoder_info[2] = p[2] << 8 | p[3];
      /* Summary: If the first pack contains the beginning of 2 A52 frames( decoder_info[1]=2),
       *            the PTS value applies to the first A52 frame.
       *          The second A52 frame will have no PTS value.
       *          The start of the next pack will contain the rest of the second A52 frame and thus, no PTS value.
       *          The third A52 frame will have the PTS value from the second pack.
       *
       *          If the first pack contains the beginning of 1 frame( decoder_info[1]=1 ),
       *            this first pack must then contain a certain amount of the previous A52 frame.
       *            the PTS value will not apply to this previous A52 frame,
       *            the PTS value will apply to the beginning of the frame that begins in this pack.
       *
       * LPCM note: The first_access_unit_pointer will point to
       *            the PCM sample within the pack at which the PTS values applies.
       *
       * Example to values in a real stream, each line is a pack (dvd sector): -
       * decoder_info[1], decoder_info[2],  PTS
       * 2                   5             125640
       * 1                1061             131400
       * 1                 581             134280
       * 2                 101             137160
       * 1                1157             142920
       * 1                 677             145800
       * 2                 197             148680
       * 1                1253             154440
       * 1                 773             157320
       * 2                 293             160200
       * 1                1349             165960
       * 1                 869             168840
       * 2                 389             171720
       * 1                1445             177480
       * 1                 965             180360
       * 1                 485             183240
       * 2                   5             186120
       * 1                1061             191880
       * 1                 581             194760
       * 2                 101             197640
       * 1                1157             203400
       * 1                 677             206280
       * 2                 197             209160
       * Notice the repeating pattern of both decoder_info[1] and decoder_info[2].
       * The resulting A52 frames will have these PTS values: -
       *  PTS
       * 125640
       *      0
       * 131400
       * 134280
       * 137160
       *      0
       * 142920
       * 145800
       * 148680
       *      0
       * 154440
       * 157320
       * 160200
       *      0
       * 165960
       * 168840
       * 171720
       *      0
       * 177480
       * 180360
       * 183240
       * 186120
       *      0
       * 191880
       * 194760
       * 197640
       *      0
       * 203400
       * 206280
       * 209160
       *      0 (Partial A52 frame.)
       */

      buf->content   = p+4;
      buf->size      = this->packet_len-4;
      if (track & 0x8) {
        buf->type      = BUF_AUDIO_DTS + (track & 0x07); /* DVDs only have 8 tracks */
      } else {
        buf->type      = BUF_AUDIO_A52 + track;
      }
      buf->pts       = this->pts;
      if( !this->preview_mode )
        check_newpts( this, this->pts, PTS_AUDIO );

      if(this->audio_fifo) {
	this->audio_fifo->put (this->audio_fifo, buf);
        lprintf ("A52 PACK put on fifo\n");

        return -1;
      } else {
	buf->free_buffer(buf);
        return -1;
      }

    } else if ((p[0]&0xf0) == 0xa0) {

      int pcm_offset;
      int number_of_frame_headers;
      int first_access_unit_pointer;
      int audio_frame_number;
      int bits_per_sample;
      int sample_rate;
      int num_channels;
      int dynamic_range;

      /*
       * found in http://members.freemail.absa.co.za/ginggs/dvd/mpeg2_lpcm.txt
       * appears to be correct.
       */

      track = p[0] & 0x0F;
      number_of_frame_headers = p[1];
      /* unknown = p[2]; */
      first_access_unit_pointer = p[3];
      audio_frame_number = p[4];

      /*
       * 000 => mono
       * 001 => stereo
       * 010 => 3 channel
       * ...
       * 111 => 8 channel
       */
      num_channels = (p[5] & 0x7) + 1;
      sample_rate = p[5] & 0x10 ? 96000 : 48000;
      switch ((p[5]>>6) & 3) {
      case 3: /* illegal, use 16-bits? */
      default:
	xprintf (this->stream->xine, XINE_VERBOSITY_DEBUG,
		 "illegal lpcm sample format (%d), assume 16-bit samples\n", (p[5]>>6) & 3 );
      case 0: bits_per_sample = 16; break;
      case 1: bits_per_sample = 20; break;
      case 2: bits_per_sample = 24; break;
      }
      dynamic_range = p[6];

      /* send lpcm config byte */
      buf->decoder_flags |= BUF_FLAG_SPECIAL;
      buf->decoder_info[1] = BUF_SPECIAL_LPCM_CONFIG;
      buf->decoder_info[2] = p[5];

      pcm_offset = 7;

      buf->content   = p+pcm_offset;
      buf->size      = this->packet_len-pcm_offset;
      buf->type      = BUF_AUDIO_LPCM_BE + track;
      buf->pts       = this->pts;
      if( !this->preview_mode )
        check_newpts( this, this->pts, PTS_AUDIO );

      if(this->audio_fifo) {
	this->audio_fifo->put (this->audio_fifo, buf);
        lprintf ("LPCM PACK put on fifo\n");

        return -1;
      } else {
	buf->free_buffer(buf);
        return -1;
      }

    }
    /* Some new streams have been encountered.
       1) DVD+RW disc recorded with a Philips DVD recorder: -  new unknown sub-stream id of 0xff
     */
    xprintf(this->stream->xine, XINE_VERBOSITY_LOG,
	    "demux_mpeg_block:Unrecognised private stream 1 0x%02x. Please report this to xine developers.\n", p[0]);
    buf->free_buffer(buf);
    return -1;
}

static int32_t parse_video_stream(demux_mpeg_block_t *this, uint8_t *p, buf_element_t *buf) {
  int32_t result;

  result = parse_pes_for_pts(this, p, buf);
  if (result < 0) return -1;

  p += result;

  buf->content   = p;
  buf->size      = this->packet_len;
  buf->type      = BUF_VIDEO_MPEG;
  buf->pts       = this->pts;
  buf->decoder_info[0] = this->pts - this->dts;
  if( !this->preview_mode )
    check_newpts( this, this->pts, PTS_VIDEO );

  this->video_fifo->put (this->video_fifo, buf);
  lprintf ("MPEG Video PACK put on fifo\n");

  return -1;
}

static int32_t parse_audio_stream(demux_mpeg_block_t *this, uint8_t *p, buf_element_t *buf) {

  int track;
  int32_t result;

  result = parse_pes_for_pts(this, p, buf);
  if (result < 0) return -1;

  p += result;

  track = this->stream_id & 0x1f;

  buf->content   = p;
  buf->size      = this->packet_len;
  buf->type      = BUF_AUDIO_MPEG + track;
  buf->pts       = this->pts;
  if( !this->preview_mode )
      check_newpts( this, this->pts, PTS_AUDIO );

  if(this->audio_fifo) {
    this->audio_fifo->put (this->audio_fifo, buf);
    lprintf ("MPEG Audio PACK put on fifo\n");

  } else {
    buf->free_buffer(buf);
  }

  return -1;
}

static int demux_mpeg_block_send_chunk (demux_plugin_t *this_gen) {

  demux_mpeg_block_t *this = (demux_mpeg_block_t *) this_gen;

  demux_mpeg_block_parse_pack(this);

  return this->status;
}

static void demux_mpeg_block_dispose (demux_plugin_t *this_gen) {

  demux_mpeg_block_t *this = (demux_mpeg_block_t *) this_gen;

  av_free (this->scratch);
  free (this);
}

static int demux_mpeg_block_get_status (demux_plugin_t *this_gen) {
  demux_mpeg_block_t *this = (demux_mpeg_block_t *) this_gen;

  return this->status;
}

static void demux_mpeg_block_send_headers (demux_plugin_t *this_gen) {

  demux_mpeg_block_t *this = (demux_mpeg_block_t *) this_gen;

  this->video_fifo  = this->stream->video_fifo;
  this->audio_fifo  = this->stream->audio_fifo;

  /*
   * send start buffer
   */

  _x_demux_control_start(this->stream);

  this->status = DEMUX_OK;

  _x_stream_info_set(this->stream, XINE_STREAM_INFO_HAS_VIDEO, 1);
  _x_stream_info_set(this->stream, XINE_STREAM_INFO_HAS_AUDIO, 1);
  _x_stream_info_set(this->stream, XINE_STREAM_INFO_BITRATE, this->rate * 50 * 8);
}


static int demux_mpeg_block_seek (demux_plugin_t *this_gen,
				   off_t start_pos, int start_time, int playing) {

  demux_mpeg_block_t *this = (demux_mpeg_block_t *) this_gen;

  /*
   * now start demuxing
   */
  this->send_newpts = 1;
  if( !playing ) {

    this->buf_flag_seek = 0;
    this->status   = DEMUX_OK ;
    this->last_pts[0]   = 0;
    this->last_pts[1]   = 0;
  } else {
    this->buf_flag_seek = 1;
    _x_demux_flush_engine(this->stream);
  }

  return this->status;
}

static int demux_mpeg_block_get_stream_length (demux_plugin_t *this_gen) {

  demux_mpeg_block_t *this = (demux_mpeg_block_t *) this_gen;
  /*
   * find input plugin
   */

    return 0;
}

static uint32_t demux_mpeg_block_get_capabilities(demux_plugin_t *this_gen) {
  return DEMUX_CAP_NOCAP;
}

static int demux_mpeg_block_get_optional_data(demux_plugin_t *this_gen,
					void *data, int data_type) {
  return DEMUX_OPTIONAL_UNSUPPORTED;
}

static demux_plugin_t *open_plugin (demux_class_t *class_gen, xine_stream_t *stream,
                                   input_plugin_t *input_gen) {

  input_plugin_t     *input = (input_plugin_t *) input_gen;
  demux_mpeg_block_t *this;

  this         = calloc(1, sizeof(demux_mpeg_block_t));
  this->stream = stream;
  this->input  = input;

  this->demux_plugin.send_headers      = demux_mpeg_block_send_headers;
  this->demux_plugin.send_chunk        = demux_mpeg_block_send_chunk;
  this->demux_plugin.seek              = demux_mpeg_block_seek;
  this->demux_plugin.dispose           = demux_mpeg_block_dispose;
  this->demux_plugin.get_status        = demux_mpeg_block_get_status;
  this->demux_plugin.get_stream_length = demux_mpeg_block_get_stream_length;
  this->demux_plugin.get_capabilities  = demux_mpeg_block_get_capabilities;
  this->demux_plugin.get_optional_data = demux_mpeg_block_get_optional_data;
  this->demux_plugin.demux_class       = class_gen;

  this->scratch    = av_mallocz(4096);
  this->status     = DEMUX_FINISHED;

  return &this->demux_plugin;
}

#if DEMUXER_PLUGIN_IFACE_VERSION < 27
static const char *get_description (demux_class_t *this_gen) {
  return "DVD/VOB demux plugin";
}

static const char *get_identifier (demux_class_t *this_gen) {
  return "MPEG_BLOCK";
}

static const char *get_extensions (demux_class_t *this_gen) {
  return "vob";
}

static const char *get_mimetypes (demux_class_t *this_gen) {
  return NULL;
}

static void class_dispose (demux_class_t *this_gen) {

  demux_mpeg_block_class_t *this = (demux_mpeg_block_class_t *) this_gen;

  free (this);
}
#endif

static void *init_plugin (xine_t *xine, void *data) {

  demux_mpeg_block_class_t     *this;

  this         = calloc(1, sizeof(demux_mpeg_block_class_t));
  this->config = xine->config;
  this->xine   = xine;

  this->demux_class.open_plugin     = open_plugin;
#if DEMUXER_PLUGIN_IFACE_VERSION < 27
  this->demux_class.get_description = get_description;
  this->demux_class.get_identifier  = get_identifier;
  this->demux_class.get_mimetypes   = get_mimetypes;
  this->demux_class.get_extensions  = get_extensions;
  this->demux_class.dispose         = class_dispose;
#else
  this->demux_class.description     = N_("DVD/VOB demux plugin");
  this->demux_class.identifier      = "MPEG_BLOCK";
  this->demux_class.mimetypes       = NULL;
  this->demux_class.extensions      = "vob vcd:/ dvd:/ pvr:/";
  this->demux_class.dispose         = default_demux_class_dispose;
#endif

  return this;
}

/*
 * exported plugin catalog entry
 */
static const demuxer_info_t demux_info_mpeg_block = {
  10                       /* priority */
};

const plugin_info_t xine_plugin_info[] EXPORTED = {
  /* type, API, "name", version, special_info, init_function */  
#if DEMUXER_PLUGIN_IFACE_VERSION < 27
  { PLUGIN_DEMUX, 26, "mpeg_block", XINE_VERSION_CODE, &demux_info_mpeg_block, init_plugin },
#if DEMUXER_PLUGIN_IFACE_VERSION >= 27
  { PLUGIN_DEMUX, 27, "mpeg_block", XINE_VERSION_CODE, &demux_info_mpeg_block, init_plugin },
#endif
  { PLUGIN_NONE, 0, "", 0, NULL, NULL }
};