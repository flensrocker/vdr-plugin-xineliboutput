
XINEDMXPLUGIN      = xineplug_dmx_mpeg_ts_hdmv.so
OBJS_XINEDMXPLUGIN = demux_ts.o

LIBS_XINE       = $(shell pkg-config libxine --libs)
CFLAGS         += $(shell pkg-config libxine --cflags)
CFLAGS         += -Ixine
CFLAGS         += -O2 -fPIC
XINEPLUGINDIR   = $(shell pkg-config libxine --variable=plugindir)
DESTDIR         = /
INSTALL         = install
LDFLAGS        += -shared -fvisibility=hidden

all: $(XINEDMXPLUGIN)

$(XINEDMXPLUGIN): $(OBJS_XINEDMXPLUGIN)
	$(CC) $(CFLAGS) $(LDFLAGS) $(LIBS_XINE) -o $@ $(OBJS_XINEDMXPLUGIN)

clean:
	@rm -rf *.o *.so *~

install: all
	@echo Installing $(DESTDIR)/$(XINEPLUGINDIR)/$(XINEDMXPLUGIN)
	@-rm -rf $(DESTDIR)/$(XINEPLUGINDIR)/$(XINEDMXPLUGIN)
	@$(INSTALL) -m 0644 $(XINEDMXPLUGIN) $(DESTDIR)/$(XINEPLUGINDIR)/$(XINEDMXPLUGIN)
