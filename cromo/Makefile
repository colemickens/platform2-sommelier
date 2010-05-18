# Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

PKG_CONFIG ?= pkg-config
INCLUDE_DIRS = $(shell $(PKG_CONFIG) --cflags gobject-2.0 \
	       dbus-1 dbus-glib-1 dbus-c++-1)
CPPFLAGS ?= $(INCLUDE_DIRS) $($(*F)_CPPFLAGS)
CXXFLAGS += -Wall -Werror -g -O0 $($(*F)_CXXFLAGS)
LDLIBS = $(shell $(PKG_CONFIG) --libs gobject-2.0 \
	 dbus-1 dbus-glib-1 dbus-c++-1) -lgflags -ldl
LDFLAGS = -Wl,--dynamic-list-cpp-typeinfo,--dynamic-list=$(SYMFILE)

DBUSXX_XML2CPP = dbusxx-xml2cpp
SYMFILE=symfile

CROMO = cromo
PLUGINS = dummy_modem.so
PLUGINDIR ?= /usr/lib/cromo/plugins
DBUSDATADIR ?= /etc/dbus-1/system.d
DBUSCONFIGFILE ?= org.chromium.ModemManager.conf

plugin_manager_CPPFLAGS=-DPLUGINDIR=\"$(PLUGINDIR)\"
dummy_modem_CPPFLAGS = -fPIC
dummy_modem_handler_CPPFLAGS = -fPIC

XMLFILES = mm-manager.xml \
	mm-mobile-error.xml \
	mm-modem-cdma.xml \
	mm-modem-connect-error.xml \
	mm-modem-error.xml \
	mm-modem-gsm-card.xml \
	mm-modem-gsm-contacts.xml \
	mm-modem-gsm-hso.xml \
	mm-modem-gsm-network.xml \
	mm-modem-gsm-sms.xml \
	mm-modem-gsm.xml \
	mm-modem-simple.xml \
	mm-modem.xml \
	mm-serial-error.xml

GLUE_HEADERS = $(XMLFILES:mm-%.xml=%_server_glue.h)

SRCS = main.cc \
       cromo_server.cc \
       modem_handler.cc \
       plugin_manager.cc
OBJS = $(SRCS:.cc=.o)

dummy_SRCS = dummy_modem_handler.cc \
	     dummy_modem.cc
dummy_OBJS = $(dummy_SRCS:.cc=.o)

all: $(CROMO) $(PLUGINS)

%_server_glue.h: $(ROOT)/usr/lib/mm/mm-%.xml
	$(DBUSXX_XML2CPP) $^ --adaptor=$@

$(OBJS): $(GLUE_HEADERS)
$(dummy_OBJS): $(GLUE_HEADERS)

$(CROMO): $(OBJS) $(SYMFILE)
	$(CXX) $(LDFLAGS) $(OBJS) $(LDLIBS) -o $@

dummy_modem.so: $(dummy_OBJS)
	$(CXX) -shared -o $@ $^

# Plugins, which are built and loaded separately from the main
# executable, must subclass ModemHandler, so we must export
# the symbols defined by that class.
$(SYMFILE): modem_handler.o
	@nm --defined-only $< | grep 'T _ZN12ModemHandler' | \
	    awk 'BEGIN {print "{"} {print "  " $$3";"} END {print "};"}' >$@

EXPORT_HEADERS=$(GLUE_HEADERS) modem_handler.h cromo_server.h plugin.h

install-headers: $(GLUE_HEADERS)
	install -d $(DESTDIR)/usr/include/cromo
	install -t $(DESTDIR)/usr/include/cromo $(EXPORT_HEADERS)

install-programs:
	install -d $(DESTDIR)$(PLUGINDIR) $(DESTDIR)$(DBUSDATADIR) \
		$(DESTDIR)/usr/sbin
	install -t $(DESTDIR)/usr/sbin $(CROMO)
	install -t $(DESTDIR)$(DBUSDATADIR) -m 0644 $(DBUSCONFIGFILE)

install: install-headers install-programs

.PHONY: clean
clean:
	$(RM) $(CROMO) *.so *.o *glue.h .*.d core a.out

# Boilerplate for automatically calculating dependencies
#.%.d: %.cc
#	$(CXX) -MM $(CPPFLAGS) $(CXXFLAGS) -MF $@ -MT $(<:.cc=.o) -MT $@ $<

#depfiles = $(patsubst %.cc,.%.d,$(SRCS)))
#$(depfiles): $(GLUE_HEADERS)

#ifneq ($(MAKECMDGOALS),clean)
#include $(depfiles)
#endif
