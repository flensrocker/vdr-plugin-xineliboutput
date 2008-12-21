/*
 * ts.h: MPEG-TS header definitions
 *
 * See the main source file 'xineliboutput.c' for copyright information and
 * how to reach the author.
 *
 * $Id: ts.h,v 1.1 2008-12-21 12:39:01 phintuka Exp $
 *
 */

#ifndef _XINELIBOUTPUT_TS_H_
#define _XINELIBOUTPUT_TS_H_

#undef TS_SYNC_BYTE
#undef TS_SIZE
#undef TS_PAYLOAD_EXISTS
#undef TS_ADAPT_FIELD_EXISTS
#undef TS_PAYLOAD_START
#undef TS_ERROR
#undef TS_PID_MASK_HI

/*
 * Constants
 */

#define TS_SYNC_BYTE          0x47
#define TS_SIZE               188
#define TS_ADAPT_FIELD_EXISTS 0x20
#define TS_PAYLOAD_EXISTS     0x10
#define TS_PAYLOAD_START      0x40
#define TS_ERROR              0x80
#define TS_PID_MASK_HI        0x1F

#define TS_HAS_PAYLOAD(ts)      ((ts)[3] & TS_PAYLOAD_EXISTS)
#define TS_PAYLOAD_START(ts)    ((ts)[1] & TS_PAYLOAD_START)
#define TS_ERROR(ts)            ((ts)[1] & TS_ERROR)
#define TS_PID(ts)            ((((ts)[1] & TS_PID_MASK_HI) << 8) + (ts)[2])
#define TS_PAYLOAD_OFFSET(ts)  (((ts)[3] & TS_ADAPT_FIELD_EXISTS) ? (ts)[4] + 5 : 4)

#define TS_PAYLOAD(ts)          ((ts) + TS_PAYLOAD_OFFSET(ts))
#define TS_PAYLOAD_SIZE(ts)     (TS_SIZE - TS_PAYLOAD_OFFSET(ts))

#define DATA_IS_TS(data)        ((data)[0] == TS_SYNC_BYTE)

/*
 * simple ES parsers
 */

typedef struct ts_state_s ts_state_t;

ts_state_t *ts_state_init(int buffer_size);
void        ts_state_reset(ts_state_t *ts);

int64_t ts_get_pts(ts_state_t *ts, const uint8_t *data);
int     ts_get_picture_type(ts_state_t *ts, const uint8_t *data);
int     ts_get_video_size(ts_state_t *ts, const uint8_t *data, video_size_t *size, int h264);


#endif // _XINELIBOUTPUT_TS_H_
