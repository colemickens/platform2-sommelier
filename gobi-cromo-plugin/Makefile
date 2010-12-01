# Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

PKG_CONFIG ?= pkg-config
INCLUDE_DIRS = $(shell $(PKG_CONFIG) --cflags dbus-c++-1 glib-2.0)

CXXFLAGS += -fPIC -I$(INCLUDE_DIRS) -Wall -Werror -g -O0
CFLAGS := $(CXXFLAGS)
LDLIBS += -lbase -lQCWWAN2k -lglog -lpthread -lrt -ludev -lmetrics

DBUSXX_XML2CPP = dbusxx-xml2cpp

PLUGINDEST ?= $(DESTDIR)/usr/lib/cromo/plugins
UDEVRULES = 77-cromo-gobi-device-blacklist.rules \
	77-cromo-gobi-device.rules
UDEVRULES_DIR = $(DESTDIR)/lib/udev/rules.d

GOBI_PLUGIN = gobi.so

GLUE_HEADERS = modem_gobi_server_glue.h

TESTS = gobi_sdk_wrapper_unittest \
	gobi_modem_unittest # builds slower than rest of project put together

SRCS = device_watcher.cc \
       gobi_modem_handler.cc \
       gobi_modem.cc \
       gobi_sdk_wrapper.cc

OBJS = $(SRCS:.cc=.o)

all: $(GOBI_PLUGIN) $(TESTS) gobi-modem-reset

$(GOBI_PLUGIN) : $(OBJS)
	$(CXX) -shared $^ $(LDFLAGS) $(LDLIBS) -o $@

%unittest : LDLIBS += -lgtest -lgmock -lcromo -lchromeos \
	    $(shell $(PKG_CONFIG) --libs gobject-2.0 dbus-1 dbus-glib-1 \
	      dbus-c++-1)

gobi-modem-reset : gobi-modem-reset.c

gobi_modem_unittest : $(OBJS) gobi_modem_unittest.o

gobi_sdk_wrapper_unittest : gobi_sdk_wrapper.o

%_server_glue.h: %.xml
	$(DBUSXX_XML2CPP) $^ --adaptor=$@

install: $(GOBI_PLUGIN) gobi-modem-reset
	install -d $(PLUGINDEST) $(UDEVRULES_DIR)
	install $(GOBI_PLUGIN) $(PLUGINDEST)
	install $(UDEVRULES) $(UDEVRULES_DIR)
	install -D modem_set_carrier $(DESTDIR)/usr/bin/modem_set_carrier
	install -D mm.sh $(DESTDIR)/usr/lib/mm.sh
	install -D modem $(DESTDIR)/usr/bin/modem
	# 04750 = setuid, rwxr-x---.
	install -m4750 --group=cromo -D gobi-modem-reset \
	  $(DESTDIR)/usr/bin/gobi-modem-reset

.PHONY: clean
clean:
	$(RM) $(GOBI_PLUGIN) $(TESTS) *.o .*.d core a.out gobi-modem-reset

.%.d: %.cc
	$(CXX) -MM $(CPPFLAGS) $(CXXFLAGS) -MF $@ -MT $(<:.cc=.o) -MT $@ $<

depfiles = $(patsubst %.cc,.%.d,$(SRCS))

$(depfiles): $(GLUE_HEADERS)

ifneq ($(MAKECMDGOALS),clean)
-include $(depfiles)
endif
