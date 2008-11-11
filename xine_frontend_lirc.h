/*
 * xine_frontend_lirc.h:
 *
 * See the main source file 'xineliboutput.c' for copyright information and
 * how to reach the author.
 *
 * $Id: xine_frontend_lirc.h,v 1.2 2008-11-11 20:48:13 phintuka Exp $
 *
 */

#ifndef XINE_FRONTEND_LIRC_H
#define XINE_FRONTEND_LIRC_H

struct fe_s;

void lirc_start(struct fe_s *fe, char *lirc_dev, int repeat_emu);
void lirc_stop(void);

#endif /* XINE_FRONTEND_LIRC_H */
