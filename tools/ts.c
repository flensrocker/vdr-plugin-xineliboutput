/*
 * ts.c: MPEG-TS
 *
 * See the main source file 'xineliboutput.c' for copyright information and
 * how to reach the author.
 *
 * $Id: ts.c,v 1.1 2009-01-27 09:24:34 phintuka Exp $
 *
 */

#ifndef LOGDBG
# include "../logdefs.h"
#endif

#include "ts.h"
#include "pes.h"

/*
 * ts_compute_crc32()
 *
 * taken from xine-lib demux_ts.c
 */
static uint32_t ts_compute_crc32(uint8_t *data, uint32_t length, uint32_t crc32)
{
  static uint32_t crc32_table[256];
  static uint     init_done = 0;

  if (!init_done) {
    uint32_t  i, j, k;
    init_done = 1;
    for (i = 0 ; i < 256 ; i++) {
      k = 0;
      for (j = (i << 24) | 0x800000 ; j != 0x80000000 ; j <<= 1)
        k = (k << 1) ^ (((k ^ j) & 0x80000000) ? 0x04c11db7 : 0);
      crc32_table[i] = k;
    }
  }

  uint32_t i;
  for(i = 0; i < length; i++)
    crc32 = (crc32 << 8) ^ crc32_table[(crc32 >> 24) ^ data[i]];

  return crc32;
}

/*
 * parse_pat()
 *
 * - parse PAT for PMT pid and program number
 *
 * modified from xine-lib demux_ts.c
 */
int ts_parse_pat(pat_data_t *pat, uint8_t *pkt)
{
  uint8_t *original_pkt = pkt;

  if (! ts_PAYLOAD_START(pkt)) {
    LOGMSG ("parse_pat: PAT without payload unit start indicator");
    return 0;
  }

  /* jump to payload */
  pkt += pkt[4]; /* pointer */
  if (pkt - original_pkt > TS_SIZE) {
    LOGMSG("parse_pat: PAT with invalid pointer");
    return 0;
  }

  uint     section_syntax_indicator = ((pkt[6] >> 7) & 0x01) ;
  uint     section_length           = ((pkt[6] & 0x03) << 8) | pkt[7];
/*uint     transport_stream_id      = (pkt[8]  << 8) | pkt[9];*/
/*uint     version_number           = (pkt[10] >> 1) & 0x1f;*/
  uint     current_next_indicator   = pkt[10] & 0x01;
  uint     section_number           = pkt[11];
  uint     last_section_number      = pkt[12];
  uint32_t crc32, calc_crc32;

  crc32 =  pkt[section_length + 4] << 24;
  crc32 |= pkt[section_length + 5] << 16;
  crc32 |= pkt[section_length + 6] << 8;
  crc32 |= pkt[section_length + 7] ;

  if ((section_syntax_indicator != 1) || !(current_next_indicator)) {
    LOGMSG("parse_pat: ssi error");
    return 0;
  }

  if (pkt - original_pkt > TS_SIZE-4 - 1 - 3 - section_length) {
    LOGMSG("parse_pat: unsupported PAT does not fit to single TS packet");
    return 0;
  }

  if ((section_number != 0) || (last_section_number != 0)) {
    LOGMSG("parse_pat: unsoupported PAT consists of multiple (%d) sections", last_section_number);
    return 0;
  }

  /* Check CRC */
  calc_crc32 = ts_compute_crc32 (pkt + 5, section_length + 3 - 4, 0xffffffff);
  if (crc32 != calc_crc32) {
    LOGMSG("parse_pat: invalid CRC");
    return 0;
  }

  /*
   * Process all programs in the program loop
   */

  uint8_t *program;
  uint     program_count;

  program_count = 0;
  for (program = pkt + 13;
       program < pkt + 13 + section_length - 9;
       program += 4) {
    uint program_number = (program[0] << 8) | program[1];
    uint pmt_pid        = ((program[2] & 0x1f) << 8) | program[3];

    /* skip NIT pids */
    if (program_number == 0x0000)
      continue;

    pat->program_number[program_count] = program_number;
    pat->pmt_pid[program_count] = pmt_pid;

    LOGDBG("PAT acquired count=%d programNumber=0x%04x pmtPid=0x%04x",
           program_count,
           pat->program_number[program_count],
           pat->pmt_pid[program_count]);

    program_count++;
  }

  pat->program_number[program_count] = 0;

  return program_count;
}


/*
 * ts_state_t
 */

struct ts_state_s {

  uint8_t  pusi_seen;

  uint8_t  inside_pes; /* Scanning ES (PES start code seen and skippped) */

  uint32_t buf_len;    /* bytes queued */
  uint32_t buf_size;   /* buffer size */
  uint8_t  buf[0];     /* payload: partial PES / video stream header etc. */
};

ts_state_t *ts_state_init(int buffer_size)
{
  if (buffer_size < 8 * TS_SIZE)
    buffer_size = 8 * TS_SIZE;

  ts_state_t *ts = (ts_state_t*)calloc(1, sizeof(ts_state_t) + buffer_size);
  ts->buf_size = buffer_size;
  return ts;
}

void ts_state_reset(ts_state_t *ts)
{
  int buf_size = ts->buf_size;
  memset(ts, 0, sizeof(ts_state_t));
  ts->buf_size = buf_size;
}

void ts_state_dispose(ts_state_t *ts)
{
  free(ts);
}

/*
 * ts_add_payload()
 *
 * Add TS packet payload to buffer.
 *  - PUSI resets the buffer
 *  - all data before first PUSI is discarded
 */
static int ts_add_payload(ts_state_t *ts, const uint8_t *data)
{
  /* start from PUSI */
  if (!ts->pusi_seen) {
    if (!ts_PAYLOAD_START(data))
      return 0;
    ts->pusi_seen = 1;
    ts->buf_len   = 0;
  }

  if (ts->buf_len >= ts->buf_size - TS_SIZE) {
    LOGMSG("ts_add_payload: buffer full");
    ts->buf_len -= TS_SIZE;
    memcpy(ts->buf, ts->buf+TS_SIZE, ts->buf_len);
  }

  int len = ts_PAYLOAD_SIZE(data);
  if (len > 0) {
    memcpy(ts->buf + ts->buf_len, ts_GET_PAYLOAD(data), len);
    ts->buf_len += len;
  }

  return ts->buf_len;
}

/*
 * ts_skip_payload()
 */
static void ts_skip_payload(ts_state_t *ts, unsigned int n)
{
  if (n < ts->buf_len) {
    ts->buf_len -= n;
    memcpy(ts->buf, ts->buf + n, ts->buf_len);
  } else {
    ts->buf_len = 0;
  }
}

/*
 * ts_scan_startcode()
 *
 * - discard all data until startcode (00 00 01) is found
 * - returns number of bytes left
 */
static int ts_scan_startcode(ts_state_t *ts)
{
  if (ts->buf_len > 2) {
    /* scan for PES or MPEG 00 00 01 */
    unsigned int i = 0, n = ts->buf_len - 2;
    while (i < n) {
      if (ts->buf[i+2] != 1)
        i += 3;
      else if(ts->buf[i+1])
        i += 2;
      else if(ts->buf[i])
        i++;
      else
        break;
    }

    /* skip data until start code */
    ts_skip_payload(ts, i);
  }

  return ts->buf_len;
}

/*
 * ts_get_pes()
 *
 * - scan for PES start
 * - return (PES) bytes queued
 */
static int ts_get_pes(ts_state_t *ts, const uint8_t *data)
{
  if (ts_add_payload(ts, data) > 0)
    return ts_scan_startcode(ts);
  return 0;
}

/*
 * ts_get_pts()
 */

int64_t ts_get_pts(ts_state_t *ts, const uint8_t *data)
{
  int64_t pts = INT64_C(-1);
  int     cnt = ts_get_pes(ts, data);

  if (cnt > 14) {
    pts = pes_get_pts(ts->buf, ts->buf_len);

    if (pts < 0 && cnt > 2*TS_SIZE)
      ts_state_reset(ts);
  }

  return pts;
}

/*
 * ts_get_picture_type()
 */

int ts_get_picture_type(ts_state_t *ts, const uint8_t *data, int h264)
{
  int pic = NO_PICTURE;
  return pic;
}

/*
 * ts_get_video_size()
 */

int ts_get_video_size(ts_state_t *ts, const uint8_t *data, video_size_t *size, int h264)
{
  return 0;
}
