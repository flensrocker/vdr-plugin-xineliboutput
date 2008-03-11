/*
 * xine_osd_command.h:
 *
 * See the main source file 'xineliboutput.c' for copyright information and
 * how to reach the author.
 *
 * $Id: xine_osd_command.h,v 1.8 2008-03-11 15:57:57 phintuka Exp $
 *
 */

#ifndef __XINE_OSD_COMMAND_H_
#define __XINE_OSD_COMMAND_H_

#ifndef PACKED
#  define PACKED  __attribute__((packed))
#endif


#define MAX_OSD_OBJECT 50

#if defined __cplusplus
extern "C" {
#endif

typedef enum  {
  OSD_Nop         = 0,    /* Do nothing ; used to initialize delay_ms counter */
  OSD_Size        = 1,    /* Set size of VDR OSD area (usually 720x576) */
  OSD_Set_RLE     = 2,    /* Create/update OSD window. Data is rle-compressed. */
  OSD_SetPalette  = 3,    /* Modify palette of already created OSD window */ 
  OSD_Move        = 4,    /* Change x/y position of already created OSD window */ 
  OSD_Close       = 5,    /* Close OSD window */
  OSD_Set_YUV     = 6,    /* Create/update OSD window. Data is in YUV420 format. */
  OSD_Commit      = 7     /* All OSD areas have been updated, commit changes to display */
} osd_command_id_t;

#define OSDFLAG_YUV_CLUT   0x01

typedef struct xine_clut_s {
  union {
    uint8_t cb  /*: 8*/;
    uint8_t g;
  };
  union {
    uint8_t cr  /*: 8*/;
    uint8_t b;
  };
  union {
    uint8_t y   /*: 8*/;
    uint8_t r;
  };
  uint8_t alpha /*: 8*/;
} PACKED xine_clut_t; /* from xine, alphablend.h */

typedef struct xine_rle_elem_s {
  uint16_t len;
  uint16_t color;
} PACKED xine_rle_elem_t; /* from xine */

typedef struct osd_rect_s {
  uint16_t x1;
  uint16_t y1;
  uint16_t x2;
  uint16_t y2;
} osd_rect_t;

typedef struct osd_command_s {
  uint32_t cmd;      /* osd_command_id_t */

  uint32_t wnd;      /* OSD window handle */

  int64_t  pts;      /* execute at given pts */
  uint32_t delay_ms; /* execute 'delay_ms' ms after previous command (for same window). */

  uint16_t x;         /* window position, x */
  uint16_t y;         /* window position, y */
  uint16_t w;         /* window width */
  uint16_t h;         /* window height */

  uint32_t datalen;   /* size of image data, in bytes */
  uint32_t num_rle;
  union {
    xine_rle_elem_t *data; /* RLE compressed image */
    uint8_t         *raw_data;
    uint64_t         dummy01;
  };
  uint32_t colors;         /* palette size */
  union {
    xine_clut_t     *palette;  /* palette (YCrCb) */
    uint64_t         dummy02;
  };

  osd_rect_t dirty_area;
  uint8_t    flags;

} PACKED osd_command_t;


#if defined __cplusplus
}
#endif

#endif /*__XINE_OSD_COMMAND_H_*/
