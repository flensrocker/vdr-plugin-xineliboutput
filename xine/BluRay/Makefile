
XINEDMXPLUGIN      = xineplug_dmx_mpeg_ts_hdmv.so
OBJS_XINEDMXPLUGIN = demux_ts.o
XINESPUPLUGIN      = xineplug_decode_spuhdmv.so
OBJS_XINESPUPLUGIN = decode_spuhdmv.o

LIBS_XINE       = $(shell pkg-config libxine --libs)
CFLAGS         += $(shell pkg-config libxine --cflags)
CFLAGS         += -Ixine
CFLAGS         += -O2 -fPIC
XINEPLUGINDIR   = $(shell pkg-config libxine --variable=plugindir)
DESTDIR         = /
INSTALL         = install
LDFLAGS        += -shared -fvisibility=hidden

all: $(XINEDMXPLUGIN) $(XINESPUPLUGIN)

$(XINEDMXPLUGIN): $(OBJS_XINEDMXPLUGIN)
	$(CC) $(CFLAGS) $(LDFLAGS) $(LIBS_XINE) -o $@ $(OBJS_XINEDMXPLUGIN)

$(XINESPUPLUGIN): $(OBJS_XINESPUPLUGIN)
	$(CC) $(CFLAGS) $(LDFLAGS) $(LIBS_XINE) -o $@ $(OBJS_XINESPUPLUGIN)

clean:
	@rm -rf *.o *.so *~

install: all
	@echo Installing $(DESTDIR)/$(XINEPLUGINDIR)/$(XINEDMXPLUGIN)
	@-rm -rf $(DESTDIR)/$(XINEPLUGINDIR)/$(XINEDMXPLUGIN)
	@$(INSTALL) -m 0644 $(XINEDMXPLUGIN) $(DESTDIR)/$(XINEPLUGINDIR)/$(XINEDMXPLUGIN)
	@echo Installing $(DESTDIR)/$(XINEPLUGINDIR)/$(XINESPUPLUGIN)
	@-rm -rf $(DESTDIR)/$(XINEPLUGINDIR)/$(XINESPUPLUGIN)
	@$(INSTALL) -m 0644 $(XINESPUPLUGIN) $(DESTDIR)/$(XINEPLUGINDIR)/$(XINESPUPLUGIN)
