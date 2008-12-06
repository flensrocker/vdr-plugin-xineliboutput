/*
 * vo_props.h: Extended video-out capabilities and properties
 *
 * See the main source file 'xineliboutput.c' for copyright information and
 * how to reach the author.
 *
 * $Id: vo_props.h,v 1.2 2008-12-06 15:17:33 phintuka Exp $
 *
 */

#ifndef XINELIBOUTPUT_VO_PROPS_H_
#define XINELIBOUTPUT_VO_PROPS_H_


/* output can scale OSD */
#define VO_CAP_OSDSCALING  0x01000000

/* enable/disable OSD scaling */
#define VO_PROP_OSD_SCALING 0x1001
/* OSD width */
#define VO_PROP_OSD_WIDTH   0x1002
/* OSD height */
#define VO_PROP_OSD_HEIGHT  0x1003

/* VDR OSD , hili_rgb_clut */
#define VDR_OSD_MAGIC       -9999


#endif /* XINELIBOUTPUT_VO_PROPS_H_ */
