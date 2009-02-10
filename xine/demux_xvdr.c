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
 * demultiplexer for xineliboutput (xvdr)
 */

#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

#define LOG_MODULE "demux_xvdr"

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

typedef struct demux_xvdr_s {
  demux_plugin_t        demux_plugin;

  xine_stream_t        *stream;
  fifo_buffer_t        *audio_fifo;
  fifo_buffer_t        *video_fifo;

  input_plugin_t       *input;

  int                   status;

  char                  mrl[256];

  int64_t               last_pts[2];
  int                   send_newpts;
  int                   buf_flag_seek;
  uint32_t              packet_len;
  int64_t               pts;
  int64_t               dts;
  uint32_t              stream_id;
  int32_t               mpeg1;

} demux_xvdr_t ;

typedef struct {

  demux_class_t     demux_class;

  /* class-wide, global variables here */

  xine_t           *xine;
  config_values_t  *config;
} demux_xvdr_class_t;

static int32_t parse_video_stream(demux_xvdr_t *this, uint8_t *p, buf_element_t *buf);
static int32_t parse_audio_stream(demux_xvdr_t *this, uint8_t *p, buf_element_t *buf);
static int32_t parse_private_stream_1(demux_xvdr_t *this, uint8_t *p, buf_element_t *buf);
static int32_t parse_padding_stream(demux_xvdr_t *this, uint8_t *p, buf_element_t *buf);


static void check_newpts(demux_xvdr_t *this, int64_t pts, int video )
{
  int64_t diff = pts - this->last_pts[video];

  if (pts && (this->send_newpts || (this->last_pts[video] && abs(diff)>WRAP_THRESHOLD))) {

    if (this->buf_flag_seek) {
      _x_demux_control_newpts(this->stream, pts, BUF_FLAG_SEEK);
      this->buf_flag_seek = 0;
    } else {
      _x_demux_control_newpts(this->stream, pts, 0);
    }
    this->send_newpts = 0;

    this->last_pts[1-video] = 0;
  }

  if (pts)
    this->last_pts[video] = pts;
}

static void demux_xvdr_parse_pack (demux_xvdr_t *this)
{
  buf_element_t *buf = NULL;
  uint8_t       *p;
  int32_t        result;

  lprintf ("read_block\n");

  buf = this->input->read_block (this->input, this->video_fifo, 8128);

  if (buf==NULL) {
    if (errno == EAGAIN)
      return;
    this->status = DEMUX_FINISHED;
    return;
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

  /*while(p < (buf->content + this->blocksize))*/ {
    if (p[0] || p[1] || (p[2] != 1)) {
      xprintf (this->stream->xine, XINE_VERBOSITY_DEBUG,
	       "demux_xvdr: error! %02x %02x %02x (should be 0x000001)\n", p[0], p[1], p[2]);
      xprintf (this->stream->xine, XINE_VERBOSITY_DEBUG, "demux_xvdr: bad block. skipping.\n");
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

static int32_t parse_padding_stream(demux_xvdr_t *this, uint8_t *p, buf_element_t *buf)
{
  buf->free_buffer (buf);
  return -1;
}

/* FIXME: Extension data is not parsed, and is also not skipped. */

static int32_t parse_pes_for_pts(demux_xvdr_t *this, uint8_t *p, buf_element_t *buf)
{
  int32_t header_len;

  this->packet_len = p[4] << 8 | p[5];

  if (this->mpeg1) {
    header_len = 6;
    p   += 6; /* packet_len -= 6; */

    while ((p[0] & 0x80) == 0x80) {
      p++;
      header_len++;
      this->packet_len--;
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

static int32_t parse_private_stream_1(demux_xvdr_t *this, uint8_t *p, buf_element_t *buf)
{
  int track, spu_id;
  int32_t result;

  result = parse_pes_for_pts(this, p, buf);
  if (result < 0) return -1;

  p += result;

  /* DVD SPU */
  if ((p[0] & 0xE0) == 0x20) {
    spu_id = (p[0] & 0x1f);

    buf->content   = p+1;
    buf->size      = this->packet_len-1;

    buf->type      = BUF_SPU_DVD + spu_id;
    buf->decoder_flags |= BUF_FLAG_SPECIAL;
    buf->decoder_info[1] = BUF_SPECIAL_SPU_DVD_SUBTYPE;
    buf->decoder_info[2] = SPU_DVD_SUBTYPE_PACKAGE;
    buf->pts       = this->pts;

    this->video_fifo->put (this->video_fifo, buf);

    return -1;
  }

  if ((p[0]&0xF0) == 0x80) {

    track = p[0] & 0x0F; /* hack : ac3 track */

    buf->decoder_info[1] = p[1];             /* Number of frame headers */
    buf->decoder_info[2] = p[2] << 8 | p[3]; /* First access unit pointer */

    buf->content   = p+4;
    buf->size      = this->packet_len-4;
    if (track & 0x8) {
      buf->type      = BUF_AUDIO_DTS + (track & 0x07); /* DVDs only have 8 tracks */
    } else {
      buf->type      = BUF_AUDIO_A52 + track;
    }
    buf->pts       = this->pts;

    check_newpts( this, this->pts, PTS_AUDIO );

    if (this->audio_fifo) {
      this->audio_fifo->put (this->audio_fifo, buf);
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

    check_newpts( this, this->pts, PTS_AUDIO );

    if (this->audio_fifo) {
      this->audio_fifo->put (this->audio_fifo, buf);
      return -1;
    } else {
      buf->free_buffer(buf);
      return -1;
    }
  }

  buf->free_buffer(buf);
  return -1;
}

static int32_t parse_video_stream(demux_xvdr_t *this, uint8_t *p, buf_element_t *buf)
{
  int32_t result;

  result = parse_pes_for_pts(this, p, buf);
  if (result < 0) return -1;

  p += result;

  buf->content   = p;
  buf->size      = this->packet_len;
  buf->type      = BUF_VIDEO_MPEG;
  buf->pts       = this->pts;
  buf->decoder_info[0] = this->pts - this->dts;

  check_newpts( this, this->pts, PTS_VIDEO );

  this->video_fifo->put (this->video_fifo, buf);

  return -1;
}

static int32_t parse_audio_stream(demux_xvdr_t *this, uint8_t *p, buf_element_t *buf)
{
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

  check_newpts( this, this->pts, PTS_AUDIO );

  if (this->audio_fifo) {
    this->audio_fifo->put (this->audio_fifo, buf);
  } else {
    buf->free_buffer(buf);
  }

  return -1;
}

/*
 * interface
 */

static int demux_xvdr_send_chunk (demux_plugin_t *this_gen)
{
  demux_xvdr_t *this = (demux_xvdr_t *) this_gen;

  demux_xvdr_parse_pack(this);

  return this->status;
}

static void demux_xvdr_dispose (demux_plugin_t *this_gen)
{
  demux_xvdr_t *this = (demux_xvdr_t *) this_gen;

  free (this);
}

static int demux_xvdr_get_status (demux_plugin_t *this_gen)
{
  demux_xvdr_t *this = (demux_xvdr_t *) this_gen;

  return this->status;
}

static void demux_xvdr_send_headers (demux_plugin_t *this_gen)
{
  demux_xvdr_t *this = (demux_xvdr_t *) this_gen;

  this->video_fifo  = this->stream->video_fifo;
  this->audio_fifo  = this->stream->audio_fifo;

  /*
   * send start buffer
   */

  _x_demux_control_start(this->stream);

  this->status = DEMUX_OK;

  _x_stream_info_set(this->stream, XINE_STREAM_INFO_HAS_VIDEO, 1);
  _x_stream_info_set(this->stream, XINE_STREAM_INFO_HAS_AUDIO, 1);
  _x_stream_info_set(this->stream, XINE_STREAM_INFO_BITRATE, 5000000);
}


static int demux_xvdr_seek (demux_plugin_t *this_gen,
                            off_t start_pos, int start_time, int playing)
{
  demux_xvdr_t *this = (demux_xvdr_t *) this_gen;

  /*
   * now start demuxing
   */
  this->send_newpts = 1;
  if (!playing) {

    this->buf_flag_seek = 0;
    this->status        = DEMUX_OK;
    this->last_pts[0]   = 0;
    this->last_pts[1]   = 0;
  } else {
    this->buf_flag_seek = 1;
    _x_demux_flush_engine(this->stream);
  }

  return this->status;
}

/*
 * demux class
 */

static int demux_xvdr_get_stream_length (demux_plugin_t *this_gen)
{
  return 0;
}

static uint32_t demux_xvdr_get_capabilities(demux_plugin_t *this_gen)
{
  return DEMUX_CAP_NOCAP;
}

static int demux_xvdr_get_optional_data(demux_plugin_t *this_gen,
                                        void *data, int data_type)
{
  return DEMUX_OPTIONAL_UNSUPPORTED;
}

static demux_plugin_t *demux_xvdr_open_plugin (demux_class_t *class_gen,
                                               xine_stream_t *stream,
                                               input_plugin_t *input_gen)
{
LOGMSG("demux open");
  input_plugin_t *input = (input_plugin_t *) input_gen;
  demux_xvdr_t   *this;
  const char     *mrl = input->get_mrl(input);

  if (strncmp(mrl, MRL_ID ":/",       MRL_ID_LEN + 2 ) &&
      strncmp(mrl, MRL_ID "+pipe://", MRL_ID_LEN + 8) &&
      strncmp(mrl, MRL_ID "+tcp://",  MRL_ID_LEN + 7) &&
      strncmp(mrl, MRL_ID "+udp://",  MRL_ID_LEN + 7) &&
      strncmp(mrl, MRL_ID "+rtp://",  MRL_ID_LEN + 7))
    return NULL;

  this         = calloc(1, sizeof(demux_xvdr_t));
  this->stream = stream;
  this->input  = input;

  this->demux_plugin.send_headers      = demux_xvdr_send_headers;
  this->demux_plugin.send_chunk        = demux_xvdr_send_chunk;
  this->demux_plugin.seek              = demux_xvdr_seek;
  this->demux_plugin.dispose           = demux_xvdr_dispose;
  this->demux_plugin.get_status        = demux_xvdr_get_status;
  this->demux_plugin.get_stream_length = demux_xvdr_get_stream_length;
  this->demux_plugin.get_capabilities  = demux_xvdr_get_capabilities;
  this->demux_plugin.get_optional_data = demux_xvdr_get_optional_data;
  this->demux_plugin.demux_class       = class_gen;

  this->status     = DEMUX_FINISHED;

  return &this->demux_plugin;
}

#if DEMUXER_PLUGIN_IFACE_VERSION < 27
static const char *demux_xvdr_get_description (demux_class_t *this_gen)
{
  return MRL_ID " demux plugin";
}

static const char *demux_xvdr_get_identifier (demux_class_t *this_gen)
{
  return MRL_ID;
}

static const char *demux_xvdr_get_extensions (demux_class_t *this_gen)
{
  return NULL;
}

static const char *demux_xvdr_get_mimetypes (demux_class_t *this_gen)
{
  return NULL;
}

static void demux_xvdr_class_dispose (demux_class_t *this_gen)
{
  demux_xvdr_class_t *this = (demux_xvdr_class_t *) this_gen;

  free (this);
}
#endif

static void *demux_init_class (xine_t *xine, void *data)
{
  demux_xvdr_class_t     *this;
LOGMSG("demux calss init");
  this         = calloc(1, sizeof(demux_xvdr_class_t));
  this->config = xine->config;
  this->xine   = xine;

  this->demux_class.open_plugin     = demux_xvdr_open_plugin;
#if DEMUXER_PLUGIN_IFACE_VERSION < 27
  this->demux_class.get_description = demux_xvdr_get_description;
  this->demux_class.get_identifier  = demux_xvdr_get_identifier;
  this->demux_class.get_mimetypes   = demux_xvdr_get_mimetypes;
  this->demux_class.get_extensions  = demux_xvdr_get_extensions;
  this->demux_class.dispose         = demux_xvdr_class_dispose;
#else
  this->demux_class.description     = N_("XVDR demux plugin");
  this->demux_class.identifier      = MRL_ID;
  this->demux_class.mimetypes       = NULL;
  this->demux_class.extensions      = MRL_ID":/ "MRL_ID"+pipe:/ "MRL_ID"+tcp:/ "MRL_ID"+udp:/ "MRL_ID"+rtp:/";
  this->demux_class.dispose         = default_demux_class_dispose;
#endif

  return this;
}

static const demuxer_info_t demux_info_xvdr = {
  100                      /* priority */
};

