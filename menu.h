/*
 * menu.h: Main Menu
 *
 * See the main source file 'xineliboutput.c' for copyright information and
 * how to reach the author.
 *
 * $Id: menu.h,v 1.1 2006-06-03 09:50:54 phintuka Exp $
 *
 */

#ifndef __XINELIB_MENU_H
#define __XINELIB_MENU_H

#include <vdr/menuitems.h>


class cMenuXinelib : public cMenuSetupPage 
{
  private:
    int field_order;
    int compression;
    int headphone;
    int autocrop;
    
#ifdef ENABLE_SUSPEND
    int suspend;
    cOsdItem *decoder_ctrl_suspend;
#endif
#ifdef HAVE_XV_FIELD_ORDER
    cOsdItem *video_ctrl_interlace_order;
#endif
    cOsdItem *audio_ctrl_compress;

    cOsdItem *ctrl_autocrop;
    cOsdItem *ctrl_headphone;

  protected:
    virtual void Store(void);

  public:
    cMenuXinelib(void);
    virtual ~cMenuXinelib();
    virtual eOSState ProcessKey(eKeys Key);
};

#endif //__XINELIB_SETUP_MENU_H
