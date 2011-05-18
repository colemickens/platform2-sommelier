# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

PKG_CONFIG ?= pkg-config
INCLUDE_DIRS = $(shell $(PKG_CONFIG) --cflags dbus-c++-1 glib-2.0)

CXXFLAGS += -fPIC -I$(INCLUDE_DIRS) -Wall -Werror -g -O0
CFLAGS := $(CXXFLAGS)
LDLIBS += -lbase -lGobiConnectionMgmt -lGobiImageMgmt -lglog -lpthread -lrt \
          -ludev -lmetrics -lgobi3k

DBUSXX_XML2CPP = dbusxx-xml2cpp

PLUGINDEST ?= $(DESTDIR)/usr/lib/cromo/plugins
UDEVRULES = 77-cromo-gobi-device-blacklist.rules \
	77-cromo-gobi-device.rules
UDEVRULES_DIR = $(DESTDIR)/lib/udev/rules.d

GOBI_PLUGIN = gobi.so

GLUE_HEADERS = modem_gobi_server_glue.h

TESTS = gobi_sdk_wrapper_unittest \
	gobi_handle_test \
	metrics_stopwatch_unittest \
	gobi_modem_unittest

PLUGIN_SRCS = device_watcher.cc \
	      gobi_cdma_modem.cc \
	      gobi_gsm_modem.cc \
	      gobi_modem.cc \
	      gobi_2k_modem.cc \
	      gobi_3k_modem.cc \
	      gobi_modem_factory.cc \
	      gobi_modem_handler.cc \
	      gobi_sdk_wrapper.cc \
	      metrics_stopwatch.cc

PLUGIN_OBJS = $(PLUGIN_SRCS:.cc=.o)

ALL_SRCS = $(PLUGIN_SRCS) $(TEST_SRCS)

TARGETS = $(GOBI_PLUGIN) $(TESTS)
INSTALL_TARGETS = install-production

ifdef INSTALL_TESTS
	INSTALL_TARGETS += install-tests
endif

all: $(TARGETS)

$(GOBI_PLUGIN) : $(PLUGIN_OBJS)
	$(CXX) -shared $^ $(LDFLAGS) $(LDLIBS) -o $@

# Libraries that we normally get via the plugin host (cromo)
%unittest : LDLIBS += -lgtest -lgmock -lcromo -lchromeos \
	    $(shell $(PKG_CONFIG) --libs gobject-2.0 dbus-1 dbus-glib-1 \
	      dbus-c++-1)

# As few dependencies as possible
gobi_handle_test : LDLIBS=-lGobiConnectionMgmt -lpthread -lrt

gobi_handle_test : gobi_handle_test.cc

gobi_modem_unittest : $(PLUGIN_OBJS) gobi_modem_unittest.cc

gobi_sdk_wrapper_unittest : gobi_sdk_wrapper.o gobi_sdk_wrapper_unittest.cc

metrics_stopwatch_unittest : metrics_stopwatch.o metrics_stopwatch_unittest.cc

%_server_glue.h: %.xml
	$(DBUSXX_XML2CPP) $^ --adaptor=$@

install : $(INSTALL_TARGETS)

install-production:
	install -d $(PLUGINDEST) $(UDEVRULES_DIR)
	install $(GOBI_PLUGIN) $(PLUGINDEST)
	install $(UDEVRULES) $(UDEVRULES_DIR)
	install -D modem_set_carrier $(DESTDIR)/usr/bin/modem_set_carrier

install-tests:
	install -d $(DESTDIR)/usr/bin
	install $(TESTS) $(DESTDIR)/usr/bin

.PHONY: clean
clean:
	$(RM) $(GOBI_PLUGIN) $(TESTS) *.o .*.d core a.out gobi-modem-reset

.%.d: %.cc
	$(CXX) -MM $(CPPFLAGS) $(CXXFLAGS) -MF $@ -MT $(<:.cc=.o) -MT $@ $<

depfiles = $(patsubst %.cc,.%.d,$(ALL_SRCS))

$(depfiles): $(GLUE_HEADERS)

ifneq ($(MAKECMDGOALS),clean)
-include $(depfiles)
endif
