/*
 * xine_osd_command.h:
 *
 * See the main source file 'xineliboutput.c' for copyright information and
 * how to reach the author.
 *
 * $Id: xine_osd_command.h,v 1.1 2006-06-03 09:50:54 phintuka Exp $
 *
 */

#ifndef __XINE_OSD_COMMAND_H_
#define __XINE_OSD_COMMAND_H_

#define MAX_OSD_OBJECT 50

#if defined __cplusplus
extern "C" {
#endif

typedef enum {
  OSD_Nop        = 0,    /* Do nothing ; used to initialize delay_ms counter */
  OSD_Size       = 1,    /* Set size of VDR OSD area (usually 720x576) */
  OSD_Set_RLE    = 2,    /* Create/update OSD window. Data is rle-compressed. */
  OSD_SetPalette = 3,    /* Modify palette of already created OSD window */ 
  OSD_Move       = 4,    /* Change x/y position of already created OSD window */ 
  OSD_Close      = 5,    /* Close OSD window */
  OSD_Set_YUV    = 6     /* Create/update OSD window. Data is in YUV420 format. */
} osd_command_id_t;

typedef struct xine_clut_s {
  uint8_t cb    : 8;
  uint8_t cr    : 8;
  uint8_t y     : 8;
  uint8_t alpha/*foo*/ : 8;
} __attribute__((packed)) xine_clut_t; /* from xine, alphablend.h */

typedef struct xine_rle_elem_s {
  uint16_t len;
  uint16_t color;
} __attribute__((packed)) xine_rle_elem_t; /* from xine */

typedef struct osd_command_s {
  uint32_t cmd;      /* osd_command_id_t */

  uint32_t wnd;      /* OSD window handle */

  int64_t  pts;      /* execute at given pts */
  uint32_t delay_ms; /* execute 'delay_ms' ms after previous command (for same window). */

  uint16_t x;         /* window position, x */
  uint16_t y;         /* window position, y */
  uint16_t w;         /* window width */
  uint16_t h;         /* window height */

  uint32_t         datalen; /* size of image data, in bytes */
  xine_rle_elem_t *data;    /* RLE compressed image */

  uint32_t     colors;      /* palette size */
  xine_clut_t *palette;     /* palette (YCrCb) */

} __attribute__((packed)) osd_command_t;


#if defined __cplusplus
}
#endif

#endif /*__XINE_OSD_COMMAND_H_*/
