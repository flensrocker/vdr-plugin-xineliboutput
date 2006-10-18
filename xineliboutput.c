/*
 * vdr-xineliboutput: xine-lib based output device plugin for VDR
 *
 * Copyright (C) 2003-2006 Petri Hintukainen <phintuka@cc.hut.fi>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 * Or, point your browser to http://www.gnu.org/copyleft/gpl.html
 *
 *
 * xineliboutput.c: VDR Plugin interface
 *
 * $Id: xineliboutput.c,v 1.13 2006-10-18 11:39:02 phintuka Exp $
 *
 */

#include <vdr/plugin.h>

#include "logdefs.h"
#include "i18n.h"
#include "config.h"
#include "device.h"
#include "setup_menu.h"
#include "menu.h"

#if VDRVERSNUM < 10342
#  if VDRVERSNUM < 10331
#    error VDR versions < 1.3.31 are not supported !
#  endif
#  warning This plugin is not tested with VDR versions < 1.3.42 !
#endif

//---------------------------------plugin-------------------------------------

static const char *VERSION        = "1.0.0pre6";
static const char *DESCRIPTION    = "X11/xine-lib output plugin";
static const char *MAINMENUENTRY  = "Media Player (xine-lib)";

cOsdObject *g_PendingMenuAction = NULL;

class cPluginXinelibOutput : public cPlugin 
{
  private:
    // Add any member variables or functions you may need here.

  public:
    cPluginXinelibOutput(void);
    virtual ~cPluginXinelibOutput();
    virtual const char *Version(void) { return VERSION; }
    virtual const char *Description(void) { return tr(DESCRIPTION); }
    virtual const char *CommandLineHelp(void);
    virtual bool ProcessArgs(int argc, char *argv[]);
    virtual bool Initialize(void);
    virtual bool Start(void);
    virtual void Stop(void);
    //virtual void Housekeeping(void);
    virtual void MainThreadHook();
    virtual const char *MainMenuEntry(void) { return xc.hide_main_menu ? NULL : tr(MAINMENUENTRY); }
    virtual cOsdObject *MainMenuAction(void);
    virtual cMenuSetupPage *SetupMenu(void);
    virtual bool SetupParse(const char *Name, const char *Value);
};

cPluginXinelibOutput::cPluginXinelibOutput(void)
{
  // Initialize any member variables here.
  // DON'T DO ANYTHING ELSE THAT MAY HAVE SIDE EFFECTS, REQUIRE GLOBAL
  // VDR OBJECTS TO EXIST OR PRODUCE ANY OUTPUT!
}

cPluginXinelibOutput::~cPluginXinelibOutput()
{
  // Clean up after yourself!
  cXinelibDevice::Dispose();
}


const char cmdLineHelp[] =
"  -l NAME   --local=NAME   Use local frontend NAME\n"
"                           Supported frontends:\n"
"                           sxfe    (X11)\n"
"                           fbfe    (framebuffer)\n"
"                           none    (only remote frontends)\n"
"  -r PORT   --remote=PORT  Listen PORT for remote clients\n"
"                           (default "LISTEN_PORT_S")\n"
"                           none or 0 disables remote mode\n"
"  -A NAME   --audio=NAME   Use audio driver NAME for local frontend\n"
"                           Supported values:\n"
"                           auto, alsa, oss, arts, esound, none\n"
"  -V NAME   --video=NAME   Use video driver NAME for local frontend\n"
"                           Supported values:\n"
"                           for sxfe: auto, x11, xshm, xv, xvmc, xxmc, none\n"
"                           for fbfe: auto, fb, DirectFB, none\n"
#if 0
"  -L        --nolocal      No local frontend, only remote frontends\n"
"  -m M      --modeline=M   Use modeline M for local frontend\n"
"                           (example: )\n" 
"  -k        --xkeyboard    X11 keyboard in use\n"
"  -R        --noremote     No remote clients\n"
#endif
"  -f        --fullscreen   Fullscreen mode (X11)\n"
"  -w        --width=x      Window width\n"
"  -h        --height=x     Window width\n"
"  -d DISP   --display=DISP Use X11 display DISP\n"
"                           (or framebuffer device name)"
"  -P NAME   --post=NAME    Use xine post plugin NAME\n"
"                           format: pluginname[:arg=val[,arg=val]][,...]\n"
"                           example: \n"
"                           --post=upmix;tvtime:enabled=1,cheap_mode=1\n"
"  -p        --primary      Force xineliboutput to be primary device when\n"
"                           there are active frontend(s)\n"
"  -c        --exit-on-close  Exit vdr when local frontend window is closed\n"
;

const char *cPluginXinelibOutput::CommandLineHelp(void)
{
  // Return a string that describes all known command line options.
  return cmdLineHelp;
}

bool cPluginXinelibOutput::ProcessArgs(int argc, char *argv[])
{
  // Implement command line argument processing here if applicable.
  return xc.ProcessArgs(argc, argv);
}

bool cPluginXinelibOutput::Initialize(void)
{
  // Initialize any background activities the plugin shall perform.
  TRACEF("cPluginXinelibOutput::Initialize");

  RegisterI18n(Phrases);
  cXinelibDevice::Instance();
  return true;
}

bool cPluginXinelibOutput::Start(void)
{
  // Start any background activities the plugin shall perform.
  TRACEF("cPluginXinelibOutput::Start");
  return cXinelibDevice::Instance().StartDevice();
}

void cPluginXinelibOutput::MainThreadHook(void)
{
  TRACEF("cPluginXinelibOutput::MainThreadHook");
  return cXinelibDevice::Instance().MainThreadHook();
}

void cPluginXinelibOutput::Stop(void)
{
  // Start any background activities the plugin shall perform.
  TRACEF("cPluginXinelibOutput::Stop");
  return cXinelibDevice::Instance().StopDevice();
}

cOsdObject *cPluginXinelibOutput::MainMenuAction(void)
{
  // Perform the action when selected from the main VDR menu.
  TRACEF("cPluginXinelibOutput::MainMenuAction");

  if(xc.main_menu_mode == CloseOsd) {
    xc.main_menu_mode = ShowMenu;
    return NULL;
  }

  if(g_PendingMenuAction) {
    cOsdObject *tmp = g_PendingMenuAction;
    g_PendingMenuAction = NULL;
    return tmp;
  }

  if(xc.hide_main_menu)
    return NULL;

#ifdef HAVE_XV_FIELD_ORDER
  xc.field_order = xc.field_order ? 0 : 1;
  cXinelibDevice::Instance().ConfigureWindow(xc.fullscreen, xc.width, xc.height, 
					     xc.modeswitch, xc.modeline, xc.display_aspect, 
					     xc.scale_video, xc.field_order);
#endif
  return new cMenuXinelib();
}

cMenuSetupPage *cPluginXinelibOutput::SetupMenu(void)
{
  // Return a setup menu in case the plugin supports one.
  TRACEF("cPluginXinelibOutput::SetupMenu");
  return new cMenuSetupXinelib();
}

bool cPluginXinelibOutput::SetupParse(const char *Name, const char *Value)
{
  // Parse your own setup parameters and store their values.
  return xc.SetupParse(Name, Value);
}

extern "C"
void *VDRPluginCreator(void) __attribute__((visibility("default")));

VDRPLUGINCREATOR(cPluginXinelibOutput); // Don't touch this!
