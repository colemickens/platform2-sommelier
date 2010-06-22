# Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

PKG_CONFIG ?= pkg-config
INCLUDE_DIRS = $(shell $(PKG_CONFIG) --cflags dbus-c++-1)

CXXFLAGS += -fPIC -I$(INCLUDE_DIRS) -Wall -Werror -g -O0
LDLIBS += -lbase -lQCWWAN2k -lglog -lpthread -lrt -ludev

DBUSXX_XML2CPP = dbusxx-xml2cpp

PLUGINDEST ?= $(DESTDIR)/usr/lib/cromo/plugins
UDEVRULES = 77-cromo-gobi-device-blacklist.rules \
	77-cromo-gobi-device.rules
UDEVRULES_DIR = $(DESTDIR)/lib/udev/rules.d

GOBI_PLUGIN = gobi.so

GLUE_HEADERS = modem_gobi_server_glue.h

TESTS = gobi_modem_unittest

SRCS = gobi_modem_handler.cc \
	gobi_modem.cc

OBJS = $(SRCS:.cc=.o)

all: $(GOBI_PLUGIN) $(TESTS)

$(GOBI_PLUGIN) : $(OBJS)
	$(CXX) -shared $^ $(LDFLAGS) $(LDLIBS) -o $@

%unittest : %unittest.cc

%unittest : LDLIBS += -lgtest -lgmock $(shell $(PKG_CONFIG) --libs gobject-2.0 \
	 dbus-1 dbus-glib-1 dbus-c++-1)

gobi_modem_unittest : gobi_modem.cc

%_server_glue.h: %.xml
	$(DBUSXX_XML2CPP) $^ --adaptor=$@

install: $(GOBI_PLUGIN)
	install -d $(PLUGINDEST) $(UDEVRULES_DIR)
	install $(GOBI_PLUGIN) $(PLUGINDEST)
	install $(UDEVRULES) $(UDEVRULES_DIR)
# Temporary hack for dogfooding L13
	install -D record_modem $(DESTDIR)/usr/bin/record_modem

.PHONY: clean
clean:
	$(RM) $(GOBI_PLUGIN) *.o .*.d core a.out

.%.d: %.cc
	$(CXX) -MM $(CPPFLAGS) $(CXXFLAGS) -MF $@ -MT $(<:.cc=.o) -MT $@ $<

depfiles = $(patsubst %.cc,.%.d,$(SRCS))

$(depfiles): $(GLUE_HEADERS)

ifneq ($(MAKECMDGOALS),clean)
-include $(depfiles)
endif
