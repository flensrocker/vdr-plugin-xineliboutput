#
# Makefile for a Video Disk Recorder plugin
#
# $Id: Makefile,v 1.2 2006-06-04 11:00:04 phintuka Exp $

# The official name of this plugin.
# This name will be used in the '-P...' option of VDR to load the plugin.
# By default the main source file also carries this name.
#

PLUGIN = xineliboutput

###
### check for xine-lib and X11
###

XINELIBOUTPUT_FB  = 0
XINELIBOUTPUT_X11 = 0
XINELIBOUTPUT_XINEPLUGIN = $(shell (xine-config --cflags >& /dev/null && echo "1") || echo "0")
ifeq ($(XINELIBOUTPUT_XINEPLUGIN), 1)
    XINELIBOUTPUT_FB  = $(XINELIBOUTPUT_XINEPLUGIN)
    XINELIBOUTPUT_X11 = $(shell (((echo "\#include <X11/Xlib.h>";echo "int main(int c,char* v[]) {return 0;}") > testx.c && gcc -c testx.c -o testx.o >&/dev/null) && echo "1") || echo "0" ; rm -f testx.* >/dev/null)

    ifeq ($(XINELIBOUTPUT_X11), 1)
        #$(warning Detected X11)
    else
        $(warning ********************************************************)
        $(warning X11 not detected ! X11 frontends will not be compiled.)
        $(warning ********************************************************)
    endif
else
    $(warning ********************************************************)
    $(warning xine-lib not detected ! frontends will not be compiled.)
    $(warning ********************************************************)
endif

#
# Override configuration here or in ../../../Make.config
#

#XINELIBOUTPUT_X11  = 1
#XINELIBOUTPUT_FB   = 1
#XINELIBOUTPUT_XINEPLUGIN = 1
#XINELIBOUTPUT_VDRPLUGIN = 1
#ENABLE_TEST_POSTPLUGINS = 1
#$(warning ********************************************************)
#$(warning TEST PLUGINS ENABLED)
#$(warning ********************************************************)

###
### The version number of this plugin (taken from the main source file):
###

VERSION = $(shell grep 'static const char \*VERSION *=' $(PLUGIN).c | awk '{ print $$6 }' | sed -e 's/[";]//g')

###
### The C++ compiler and options:
###

CXX      ?= g++
OPTFLAGS ?= 
CXXFLAGS ?= -O3 -pipe -Wall -Woverloaded-virtual -fPIC -g 

CC     ?= gcc 
CFLAGS ?= -O3 -pipe -Wall -fPIC -g

###
### The directory environment:
###

VDRDIR = ../../..
LIBDIR = ../../lib
TMPDIR = /tmp

###
### Allow user defined options to overwrite defaults:
###

-include $(VDRDIR)/Make.config

###
### check for VDR
###

# XINELIBOUTPUT_VDRPLUGIN ?= $(shell (grep 'define VDRVERSION ' $(VDRDIR)/config.h > /dev/null && echo "1") || echo "0")
# ifeq ($(XINELIBOUTPUT_VDRPLUGIN), 0)

### The version number of VDR (taken from VDR's "config.h"):
#VDRVERSION = $(shell grep 'define VDRVERSION ' $(VDRDIR)/config.h | awk '{ print $$3 }' | sed -e 's/"//g')
VDRVERSION = $(shell sed -ne '/define VDRVERSION/ { s/^.*"\(.*\)".*$$/\1/; p }' $(VDRDIR)/config.h)
ifeq ($(strip $(VDRVERSION)),)
    $(warning ********************************************************)
    $(warning VDR not detected ! VDR plugin will not be compiled.)
    $(warning ********************************************************)
    XINELIBOUTPUT_VDRPLUGIN = 0
else
    ### The version number of VDR's plugin API (taken from VDR's "config.h"):
    #APIVERSION = $(shell grep 'define APIVERSION ' $(VDRDIR)/config.h | awk '{ print $$3 }' | sed -e 's/"//g')
    APIVERSION = $(shell sed -ne '/define APIVERSION/ { s/^.*"\(.*\)".*$$/\1/; p }' $(VDRDIR)/config.h)
    ifeq ($(strip $(APIVERSION)),)
        $(warning VDR APIVERSION missing, using VDRVERSION $(VDRVERSION) )
        APIVERSION = $(VDRVERSION)
    endif
    XINELIBOUTPUT_VDRPLUGIN = 1
endif

###
### The name of the distribution archive:
###

ARCHIVE = $(PLUGIN)-$(VERSION)
PACKAGE = vdr-$(ARCHIVE)

###
### The name of executable and libraries
###

VDRSXFE      = vdr-sxfe
VDRFBFE      = vdr-fbfe
XINEINPUTVDR = xineplug_inp_xvdr.so
XINEPOSTAUTOSCALE = xineplug_post_autoscale.so
XINEPOSTHEADPHONE = xineplug_post_headphone.so

###
### which programs and libs to build
###

ifeq ($(XINELIBOUTPUT_X11), 1)
    VDRSXFE_EXEC = $(VDRSXFE)
    ifeq ($(XINELIBOUTPUT_VDRPLUGIN), 1)
        VDRPLUGIN_SXFE_SO = lib$(PLUGIN)-sxfe.so
    else
        VDRPLUGIN_SXFE_SO =
    endif
else
    VDRSXFE_EXEC = 
    VDRPLUGIN_SXFE_SO =
endif
ifeq ($(XINELIBOUTPUT_FB), 1)
    VDRFBFE_EXEC = $(VDRFBFE)
    ifeq ($(XINELIBOUTPUT_VDRPLUGIN), 1)
        VDRPLUGIN_FBFE_SO = lib$(PLUGIN)-fbfe.so
    else
        VDRPLUGIN_FBFE_SO =
    endif
else
    VDRFBFE_EXEC = 
    VDRPLUGIN_FBFE_SO =
endif
ifeq ($(XINELIBOUTPUT_XINEPLUGIN), 1)
    XINEINPUTVDR_SO = $(XINEINPUTVDR)
    XINEPLUGINDIR   = $(shell xine-config --plugindir)
    ifeq ($(ENABLE_TEST_POSTPLUGINS), 1)
        XINEPOSTAUTOSCALE_SO = $(XINEPOSTAUTOSCALE)
        XINEPOSTHEADPHONE_SO = $(XINEPOSTHEADPHONE)
    else
        XINEPOSTAUTOSCALE_SO = 
        XINEPOSTHEADPHONE_SO = 
    endif
else
    XINEINPUTVDR_SO = 
    XINEPLUGINDIR = ./
    XINEPOSTAUTOSCALE_SO = 
    XINEPOSTHEADPHONE_SO = 
endif
ifeq ($(XINELIBOUTPUT_VDRPLUGIN), 1)
    VDRPLUGIN_SO = libvdr-$(PLUGIN).so
else
    VDRPLUGIN_SO =
endif

###
### Includes and Defines (add further entries here):
###

INCLUDES  += -I$(VDRDIR)/include
LIBS_XINE += $(shell xine-config --libs)
LIBS_X11  += -L/usr/X11R6/lib -lX11 -lXv -lXext

DEFINES   += -D_GNU_SOURCE -DPLUGIN_NAME_I18N='"$(PLUGIN)"' \
             -D_REENTRANT -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64 \
             -DPLUGINLIBDIR='"$(PLUGINLIBDIR)"' \
	     -DXINELIBOUTPUT_VERSION='"$(VERSION)"'

# check for yaegp patch
DEFINES += $(shell grep 'vidWin' \$(VDRDIR)/osd.h > /dev/null && echo "-DYAEGP_PATCH")

ifeq ($(XINELIBOUTPUT_XINEPLUGIN), 1)
    CFLAGS += $(shell xine-config --cflags) 
endif

ifeq ($(ENABLE_TEST_POSTPLUGINS), 1)
    DEFINES += -DENABLE_TEST_POSTPLUGINS
endif

#CFLAGS += -Wall -pedantic -ansi
#CXXFLAGS += -Wall -ansi -pedantic

###
### configuration
###

#DEFINES += -DENABLE_SUSPEND
#DEFINES += -DHAVE_XV_FIELD_ORDER
#DEFINES += -DSUSPEND_BY_PLAYER

###
### The object files (add further files here):
###
ifeq ($(XINELIBOUTPUT_VDRPLUGIN), 1)
  OBJS = $(PLUGIN).o device.o frontend.o osd.o config.o menu.o setup_menu.o \
         i18n.o menuitems.o media_player.o equalizer.o \
         frontend_local.o frontend_svr.o \
         tools/cxsocket.o tools/udp_pes_scheduler.o \
         tools/backgroundwriter.o
  OBJS_MPG  = black_720x576.o nosignal_720x576.o vdrlogo_720x576.o
else
  OBJS = 
  OBJS_MPG = 
endif
ifeq ($(XINELIBOUTPUT_XINEPLUGIN), 1)
else
endif
ifeq ($(XINELIBOUTPUT_X11), 1)
  OBJS_SXFE_SO = xine_sxfe_frontend.o xine/post.o
  OBJS_SXFE = xine_sxfe_frontend_standalone.o xine/post.o
else
  OBJS_SXFE_SO = 
  OBJS_SXFE = 
endif
ifeq ($(XINELIBOUTPUT_FB), 1)
  OBJS_FBFE_SO = xine_fbfe_frontend.o xine/post.o
  OBJS_FBFE = xine_fbfe_frontend_standalone.o xine/post.o
else
  OBJS_FBFE_SO = 
  OBJS_FBFE = 
endif

###
### Implicit rules:
###

%.o: %.c
	$(CXX) $(CXXFLAGS) -c $(DEFINES) $(INCLUDES) -o $@ $<

###
### Dependencies:
###

MAKEDEP = g++ -MM -MG
DEPFILE = .dependencies
$(DEPFILE): Makefile
	@$(MAKEDEP) $(DEFINES) $(INCLUDES) $(OBJS:%.o=%.c) $(OBJS_SXFE_SO:%.o=%.c) $(OBJS_FBFE_SO:%.o=%.c) > $@

-include $(DEPFILE)

DEFINES += -Wall

###
### Rules:
###

mpg2c: mpg2c.c
	$(CC) mpg2c.c -o $@

black_720x576.c: mpg2c black_720x576.mpg
	@./mpg2c black black_720x576.mpg black_720x576.c
nosignal_720x576.c: mpg2c nosignal_720x576.mpg
	@./mpg2c nosignal nosignal_720x576.mpg nosignal_720x576.c
vdrlogo_720x576.c: mpg2c vdrlogo_720x576.mpg
	@./mpg2c vdrlogo vdrlogo_720x576.mpg vdrlogo_720x576.c

xine_input_vdr.o: xine_input_vdr.c xine_input_vdr.h xine_osd_command.h
	$(CC) $(CFLAGS) -c $(DEFINES) $(INCLUDES) $(OPTFLAGS) xine_input_vdr.c
xine/post.o: xine/post.c xine/post.h
	$(CC) $(CFLAGS) -c $(DEFINES) $(INCLUDES) $(OPTFLAGS) xine/post.c -o $@
xine_post_autoscale.o: xine_post_autoscale.c
	$(CC) $(CFLAGS) -c $(DEFINES) $(INCLUDES) $(OPTFLAGS) xine_post_autoscale.c
xine_post_headphone.o: xine_post_headphone.c
	$(CC) $(CFLAGS) -c $(DEFINES) $(INCLUDES) $(OPTFLAGS) xine_post_headphone.c

xine_sxfe_frontend.o: xine_sxfe_frontend.c xine_frontend.c xine_frontend.h \
		xine_input_vdr.h xine_osd_command.h xine/post.h logdefs.h \
		xineliboutput.c
	$(CC) $(CFLAGS) -c $(DEFINES) $(INCLUDES) $(OPTFLAGS) xine_sxfe_frontend.c
xine_fbfe_frontend.o: xine_fbfe_frontend.c xine_frontend.c xine_frontend.h \
		xine_input_vdr.h xine_osd_command.h xine/post.h logdefs.h \
		xineliboutput.c
	$(CC) $(CFLAGS) -c $(DEFINES) $(INCLUDES) $(OPTFLAGS) xine_fbfe_frontend.c
xine_sxfe_frontend_standalone.o: xine_sxfe_frontend.c xine_frontend.c \
		xine_frontend.h xine_input_vdr.h xine_osd_command.h \
		xine/post.h logdefs.h xine_frontend_main.c xine_frontend_lirc.c \
		xine_frontend_lirc.c xineliboutput.c
	$(CC) $(CFLAGS) -c $(DEFINES) -DFE_STANDALONE $(INCLUDES) $(OPTFLAGS) xine_sxfe_frontend.c -o $@
xine_fbfe_frontend_standalone.o: xine_fbfe_frontend.c xine_frontend.c \
		xine_frontend.h xine_input_vdr.h xine_osd_command.h \
		xine/post.h logdefs.h xine_frontend_main.c xine_frontend_lirc.c \
		xine_frontend_lirc.c xineliboutput.c
	$(CC) $(CFLAGS) -c $(DEFINES) -DFE_STANDALONE $(INCLUDES) $(OPTFLAGS) xine_fbfe_frontend.c -o $@

#
# targets
#

all: $(VDRPLUGIN_SO) $(VDRPLUGIN_SXFE_SO) $(VDRPLUGIN_FBFE_SO) \
	$(VDRSXFE_EXEC) $(VDRFBFE_EXEC) $(XINEINPUTVDR_SO) \
	$(XINEPOSTAUTOSCALE_SO) $(XINEPOSTHEADPHONE_SO)


frontends: $(VDRSXFE_EXEC) $(VDRFBFE_EXEC) $(XINEINPUTVDR_SO) \
	$(XINEPOSTAUTOSCALE_SO) $(XINEPOSTHEADPHONE_SO)


.PHONY: all


ifeq ($(XINELIBOUTPUT_VDRPLUGIN), 1)
$(VDRPLUGIN_SO): $(OBJS) $(OBJS_MPG)
	$(CXX) $(CXXFLAGS) -shared -fvisibility=hidden $(OBJS) $(OBJS_MPG) $(LIBS) -o $@
	@-rm -rf $(LIBDIR)/$@.$(APIVERSION)
	@cp $@ $(LIBDIR)/$@.$(APIVERSION)
endif

ifeq ($(XINELIBOUTPUT_X11), 1)
$(VDRPLUGIN_SXFE_SO): $(OBJS_SXFE_SO)
	$(CC) $(CFLAGS) -shared -fvisibility=hidden $(OBJS_SXFE_SO) $(LIBS_X11) $(LIBS_XINE) -o $@
	@-rm -rf $(LIBDIR)/$(VDRPLUGIN_SXFE_SO).$(VERSION)
	@cp $@ $(LIBDIR)/$(VDRPLUGIN_SXFE_SO).$(VERSION)
$(VDRSXFE): $(OBJS_SXFE)
	$(CC) -g $(OBJS_SXFE) $(LIBS_X11) $(LIBS_XINE) -o $@
endif

ifeq ($(XINELIBOUTPUT_FB), 1)
$(VDRPLUGIN_FBFE_SO): $(OBJS_FBFE_SO)
	$(CC) $(CFLAGS) -shared -fvisibility=hidden $(OBJS_FBFE_SO) $(LIBS_XINE) -o $@
	@-rm -rf $(LIBDIR)/$(VDRPLUGIN_FBFE_SO).$(VERSION)
	@cp $@ $(LIBDIR)/$(VDRPLUGIN_FBFE_SO).$(VERSION)
$(VDRFBFE): $(OBJS_FBFE)
	$(CC) -g $(OBJS_FBFE) $(LIBS_XINE) -o $@
endif

ifeq ($(XINELIBOUTPUT_XINEPLUGIN), 1)
$(XINEINPUTVDR_SO): xine_input_vdr.o
	$(CC) -g -shared -fvisibility=hidden xine_input_vdr.o $(LIBS_XINE) -o $@
	@-rm -rf $(XINEPLUGINDIR)/$@
	@cp $@ $(XINEPLUGINDIR)/
endif

ifeq ($(ENABLE_TEST_POSTPLUGINS), 1)
$(XINEPOSTAUTOSCALE_SO): xine_post_autoscale.o
	$(CC) -g -shared -fvisibility=hidden xine_post_autoscale.o $(LIBS_XINE) -o $@
	@-rm -rf $(XINEPLUGINDIR)/$@
	@cp $@ $(XINEPLUGINDIR)/
$(XINEPOSTHEADPHONE_SO): xine_post_headphone.o
	$(CC) -g -shared -fvisibility=hidden xine_post_headphone.o $(LIBS_XINE) -o $@
	@-rm -rf $(XINEPLUGINDIR)/$@
	@cp $@ $(XINEPLUGINDIR)/
endif


install: all
ifeq ($(XINELIBOUTPUT_XINEPLUGIN), 1)
		@-rm -rf $(XINEPLUGINDIR)/$(XINEINPUTVDR)
		@-cp $(XINEINPUTVDR) $(XINEPLUGINDIR)/
endif
ifeq ($(ENABLE_TEST_POSTPLUGINS), 1)
		@-rm -rf $(XINEPLUGINDIR)/$(XINEPOSTHEADPHONE)
		@-rm -rf $(XINEPLUGINDIR)/$(XINEPOSTAUTOSCALE)
		@-cp $(XINEPOSTHEADPHONE) $(XINEPLUGINDIR)/
		@-cp $(XINEPOSTAUTOSCALE) $(XINEPLUGINDIR)/
endif
ifeq ($(XINELIBOUTPUT_FB), 1)
		@-rm -rf /usr/bin/vdr-fbfe
		@-cp vdr-fbfe /usr/bin/
endif
ifeq ($(XINELIBOUTPUT_X11), 1)
		@-rm -rf /usr/bin/vdr-sxfe
		@-cp vdr-sxfe /usr/bin/
endif


dist: clean
	@-rm -rf $(TMPDIR)/$(ARCHIVE)
	@mkdir $(TMPDIR)/$(ARCHIVE)
	@cp -a * $(TMPDIR)/$(ARCHIVE)
	@tar czf $(PACKAGE).tgz -C $(TMPDIR) $(ARCHIVE)
	@-rm -rf $(TMPDIR)/$(ARCHIVE)
	@echo Distribution package created as $(PACKAGE).tgz


clean:
	@-rm -f $(DEPFILE) *.so *.o *.tgz core* *~ *.flc *.bak \
		tools/*.o tools/*~ tools/*.flc xine/*.o xine/*~ \
		xine/*.flc $(VDR_FBFE) $(VDR_SXFE) mpg2c black_720x576.c \
		nosignal_720x576.c vdrlogo_720x576.c vdr-sxfe vdr-fbfe

