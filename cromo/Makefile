# Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

PKG_CONFIG ?= pkg-config
INCLUDE_DIRS = $(shell $(PKG_CONFIG) --cflags gobject-2.0 \
	       dbus-1 dbus-glib-1 dbus-c++-1)
CPPFLAGS ?= $(INCLUDE_DIRS) $($(*F)_CPPFLAGS)
CXXFLAGS += -Wall -Werror -g -O0 $($(*F)_CXXFLAGS)
LDLIBS = $(shell $(PKG_CONFIG) --libs gobject-2.0 gthread-2.0 \
	 dbus-1 dbus-glib-1 dbus-c++-1) -lgflags -lglog -ldl -lchromeos \
	 -lmetrics
LDLIBS_UNITTEST = $(LDLIBS) -lgtest

LDFLAGS_CROMO = -Wl,--dynamic-list-cpp-typeinfo,--dynamic-list=$(SYMFILE)

DBUSXX_XML2CPP = dbusxx-xml2cpp
SYMFILE=symfile

CROMO = cromo

# We keep utility functions in libcromo so plugin tests can use them
# without pulling in all of cromo
LIBCROMO = libcromo.a
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

# TODO(rochberg):

# Double-hack alert: The first hack is the existence of libcromo.a The
# second hack is that if we link against libcromo.a, our current
# method for exporting symbols does not export symbols for libraries.
# So we link against utilities.o, but external tests link against
# libcromo (which is currently just utilities.o)

SRCS = carrier.cc \
       cromo_server.cc \
       hooktable.cc \
       main.cc \
       modem_handler.cc \
       plugin_manager.cc \
       utilities.cc

OBJS = $(SRCS:.cc=.o)

TARGETS = $(CROMO) $(LIBCROMO) $(PLUGINS)
TESTS = cromo_server_unittest utilities_unittest

INSTALL_TARGETS = install-programs install-libs install-headers

ifdef INSTALL_TESTS
	TARGETS += $(TESTS)
	INSTALL_TARGETS += install-tests
endif


dummy_SRCS = dummy_modem_handler.cc \
	     dummy_modem.cc
dummy_OBJS = $(dummy_SRCS:.cc=.o)

all : $(TARGETS)

%unittest :
	$(CXX) $^ $(LDLIBS_UNITTEST) -o $@

%_server_glue.h: $(ROOT)/usr/lib/mm/mm-%.xml
	$(DBUSXX_XML2CPP) $^ --adaptor=$@

$(OBJS): $(GLUE_HEADERS)
$(dummy_OBJS): $(GLUE_HEADERS)

$(CROMO): $(OBJS) $(SYMFILE) $(LIBCROMO)
	$(CXX) $(LDFLAGS_CROMO) $(OBJS) $(LDLIBS) -o $@

$(LIBCROMO): utilities.o
	$(AR) q $@ $^

cromo_server_unittest: carrier.o cromo_server_unittest.o cromo_server.o hooktable.o
utilities_unittest: utilities_unittest.o $(LIBCROMO)

dummy_modem.so: $(dummy_OBJS)
	$(CXX) -shared -o $@ $^

# Export symbols used by plugins
KEEP_SYMS:= "_ZN11CromoServer \
    _ZN9HookTable \
    _ZN12ModemHandler \
    _ZN9utilities"

$(SYMFILE): cromo_server.o hooktable.o modem_handler.o $(LIBCROMO)
	@echo '{' >$@
	@for sym in "$(KEEP_SYMS)"; do \
	  nm --defined-only $^ | grep "T $${sym}" \
            | awk '{print "  "$$3";"}' >>$@ ; \
	 done
	@echo '};' >>$@
	@echo Exported symbols
	@cat $@

EXPORT_HEADERS=$(GLUE_HEADERS) modem_handler.h cromo_server.h plugin.h \
                               hooktable.h carrier.h utilities.h

install-headers: $(GLUE_HEADERS)
	install -d $(DESTDIR)/usr/include/cromo
	install -t $(DESTDIR)/usr/include/cromo $(EXPORT_HEADERS)

install-programs:
	install -d $(DESTDIR)$(PLUGINDIR) \
			   $(DESTDIR)$(DBUSDATADIR) \
			   $(DESTDIR)/usr/sbin \
			   $(DESTDIR)/usr/bin
	install -t $(DESTDIR)/usr/sbin $(CROMO)
	install -t $(DESTDIR)$(DBUSDATADIR) -m 0644 $(DBUSCONFIGFILE)
	install -t $(DESTDIR)/usr/bin mm-cromo-command

install-tests: $(TESTS)
	install -d $(DESTDIR)/usr/bin
	install -t $(DESTDIR)/usr/bin $^

install-libs: $(LIBCROMO)
	install -d $(DESTDIR)/usr/lib
	install -t $(DESTDIR)/usr/lib $(LIBCROMO)

install: $(INSTALL_TARGETS)

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
