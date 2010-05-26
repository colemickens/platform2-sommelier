# Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

PKG_CONFIG ?= pkg-config
INCLUDE_DIRS = $(shell $(PKG_CONFIG) --cflags dbus-c++-1)

CXXFLAGS += -fPIC -I$(INCLUDE_DIRS) -Wall -Werror -g -O0
LDFLAGS += -lbase -lQCWWAN2k -lglog -lpthread -lrt

PLUGINDEST ?= $(DESTDIR)/usr/lib/cromo/plugins

GOBI_PLUGIN = gobi.so

TESTS = gobi_modem_unittest

all: $(GOBI_PLUGIN) $(TESTS)


SRCS = gobi_modem_handler.cc \
	gobi_modem.cc

OBJS = $(SRCS:.cc=.o)


$(GOBI_PLUGIN) : $(OBJS)
	$(CXX) -shared $(LDFLAGS) -o $@ $^

%unittest : %unittest.cc

%unittest : LDFLAGS += -lgtest

install: $(GOBI_PLUGIN)
	install -d $(PLUGINDEST)
	install $(GOBI_PLUGIN) $(PLUGINDEST)


.PHONY: clean
clean:
	$(RM) $(GOBI_PLUGIN) *.o .*.d core a.out


# Boilerplate for automatically calculating dependencies
.%.d: %.cc
	$(CXX) -MM $(CPPFLAGS) $(CXXFLAGS) -MF $@ -MT $(<:.cc=.o) -MT $@ $<

depfiles = $(patsubst %.cc,.%.d,$(CROMO_SRCS) $(PLUGIN_SRCS))
$(depfiles): $(SRCS)
