/*
 * vo_hook.h: Intercept video driver
 *
 * See the main source file 'xineliboutput.c' for copyright information and
 * how to reach the author.
 *
 * $Id: vo_hook.h,v 1.1 2008-11-20 09:24:27 phintuka Exp $
 *
 */

#ifndef _XINELIBOUTPUT_VO_HOOK_H
#define _XINELIBOUTPUT_VO_HOOK_H

#include <xine/video_out.h>

/*
 * vo_driver_hook_t
 *
 * Used as base for video driver hooks
 */

typedef struct driver_hook_s {
  vo_driver_t     vo;             /* public part */
  vo_driver_t    *orig_driver;
} vo_driver_hook_t;

#endif /* _XINELIBOUTPUT_VO_HOOK_H */
