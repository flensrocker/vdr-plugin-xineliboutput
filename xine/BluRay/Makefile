#
# targets
#

XINEPLUGINDIR      = $(shell pkg-config libxine --variable=plugindir)

XINEDMXPLUGIN      = xineplug_dmx_mpeg_ts_hdmv.so
XINESPUPLUGIN      = xineplug_decode_spuhdmv.so

#
# compiler options
#

CFLAGS      += -O2 -fPIC
CFLAGS      += $(shell pkg-config libxine --cflags)
CFLAGS      += $(CFLAGS_BD)
DESTDIR      = /
INSTALL      = install

ifeq ($(ENABLE_INPUT_PLUGIN), yes)
CFLAGS_BD   ?= -I/usr/src/libburay/libbluray/trunk/src/
endif

#
# linker options
#

LIBS_XINE    = $(shell pkg-config libxine --libs)
LDFLAGS     += -shared -fvisibility=hidden

#
# object files
#

OBJS_XINEDMXPLUGIN   = demux_ts.o
OBJS_XINESPUPLUGIN   = decode_spuhdmv.o

#
#
#

all: $(XINEDMXPLUGIN) $(XINESPUPLUGIN) $(XINEINPUTPLUGIN)

$(XINEDMXPLUGIN): $(OBJS_XINEDMXPLUGIN)
	$(CC) $(CFLAGS) $(LDFLAGS) $(LIBS_XINE) -o $@ $(OBJS_XINEDMXPLUGIN)

$(XINESPUPLUGIN): $(OBJS_XINESPUPLUGIN)
	$(CC) $(CFLAGS) $(LDFLAGS) $(LIBS_XINE) -o $@ $(OBJS_XINESPUPLUGIN)

clean:
	@rm -rf *.o *.so *~

install: all uninstall
	@echo Installing $(DESTDIR)/$(XINEPLUGINDIR)/$(XINEDMXPLUGIN)
	@$(INSTALL) -m 0644 $(XINEDMXPLUGIN) $(DESTDIR)/$(XINEPLUGINDIR)/$(XINEDMXPLUGIN)
	@echo Installing $(DESTDIR)/$(XINEPLUGINDIR)/$(XINESPUPLUGIN)
	@$(INSTALL) -m 0644 $(XINESPUPLUGIN) $(DESTDIR)/$(XINEPLUGINDIR)/$(XINESPUPLUGIN)

uninstall:
	@echo Removing $(DESTDIR)/$(XINEPLUGINDIR)/$(XINEDMXPLUGIN)
	@-rm -rf $(DESTDIR)/$(XINEPLUGINDIR)/$(XINEDMXPLUGIN)
	@echo Removing $(DESTDIR)/$(XINEPLUGINDIR)/$(XINESPUPLUGIN)
	@-rm -rf $(DESTDIR)/$(XINEPLUGINDIR)/$(XINESPUPLUGIN)
