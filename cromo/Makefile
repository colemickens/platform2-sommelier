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
	 -lmetrics -lminijail
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
DBUSIFACEDIR ?= $(ROOT)/usr/share/dbus-1/interfaces

plugin_manager_CPPFLAGS=-DPLUGINDIR=\"$(PLUGINDIR)\"
dummy_modem_CPPFLAGS = -fPIC
dummy_modem_handler_CPPFLAGS = -fPIC

XMLFILES := mm-mobile-error.xml \
            mm-serial-error.xml \
            org.freedesktop.ModemManager.Modem.xml \
            org.freedesktop.ModemManager.Modem.Cdma.xml \
            org.freedesktop.ModemManager.Modem.Gsm.xml \
            org.freedesktop.ModemManager.Modem.Gsm.Card.xml \
            org.freedesktop.ModemManager.Modem.Gsm.Contacts.xml \
            org.freedesktop.ModemManager.Modem.Gsm.Network.xml \
            org.freedesktop.ModemManager.Modem.Gsm.SMS.xml \
            org.freedesktop.ModemManager.Modem.Gsm.Ussd.xml \
            org.freedesktop.ModemManager.Modem.Simple.xml \
            org.freedesktop.ModemManager.xml \
            org.freedesktop.DBus.Properties.xml

XMLPREFIX := \\(org.freedesktop.ModemManager\\|org.freedesktop.DBus\\)
EMPTY := manager

GLUE_HEADERS := $(foreach XML,$(XMLFILES), \
                  $(shell ./iface2header "$(XMLPREFIX)" \
                                         $(EMPTY) \
                                         $(XML)))

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
       sandbox.cc \
       sms_message.cc \
       utilities.cc

OBJS = $(SRCS:.cc=.o)

TARGETS = $(CROMO) $(LIBCROMO) $(PLUGINS)
TESTS = cromo_server_unittest sms_message_unittest utilities_unittest

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

glue: $(patsubst %,$(DBUSIFACEDIR)/%,$(XMLFILES))
	@for xml in $(XMLFILES); do \
		hdr=$$(./iface2header "$(XMLPREFIX)" $(EMPTY) $$xml); \
		echo "xml2cpp $$xml -> $$hdr"; \
		$(DBUSXX_XML2CPP) $(DBUSIFACEDIR)/$$xml --adaptor=$$hdr; \
	done

$(OBJS) cromo_server_unittest.o: glue
$(dummy_OBJS): glue

$(CROMO): $(OBJS) $(SYMFILE) $(LIBCROMO)
	$(CXX) $(LDFLAGS_CROMO) $(OBJS) $(LDLIBS) -o $@

$(LIBCROMO): cromo_server.o hooktable.o utilities.o modem_handler.o \
	     sms_message.o
	$(AR) r $@ $^

cromo_server_unittest: carrier.o cromo_server_unittest.o cromo_server.o hooktable.o
utilities_unittest: utilities_unittest.o $(LIBCROMO)
sms_message_unittest: sms_message_unittest.o $(LIBCROMO)

dummy_modem.so: $(dummy_OBJS)
	$(CXX) -shared -o $@ $^

# Export symbols used by plugins
KEEP_SYMS:= "_ZN11CromoServer \
    _ZN9HookTable \
    _ZN12ModemHandler \
    _ZN9utilities \
    _ZN10SmsMessage13CreateMessage"

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
                               hooktable.h carrier.h utilities.h modem.h \
			       sms_message.h

install-headers: glue
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
