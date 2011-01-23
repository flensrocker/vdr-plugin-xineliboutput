/*
 * xine_frontend_kbd.h:
 *
 * See the main source file 'xineliboutput.c' for copyright information and
 * how to reach the author.
 *
 * $Id: xine_frontend_kbd.h,v 1.1 2011-01-23 19:42:08 phintuka Exp $
 *
 */

#ifndef XINE_FRONTEND_KBD_H
#define XINE_FRONTEND_KBD_H

struct frontend_s;

void kbd_start(frontend_t *fe, int slave_mode);
void kbd_stop(void);

#endif /* XINE_FRONTEND_KBD_H */
