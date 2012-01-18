# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

CXXFLAGS ?= -Wall -Werror -g
PKG_CONFIG ?= pkg-config

PC_DEPS = dbus-1 dbus-glib-1 gobject-2.0 libchromeos

INCLUDES = -I.. $(shell $(PKG_CONFIG) --cflags $(PC_DEPS))
LIBS += -lpthread -lrt -lrootdev $(shell $(PKG_CONFIG) --libs $(PC_DEPS))

CXXFLAGS += $(INCLUDES)

COMMON_OBJS = image_burner_utils.o image_burner_impl.o
IMAGEBURN_OBJS = $(COMMON_OBJS) image_burn_service.o image_burner.o
BINS = image_burner image_burner_tester unittest_runner

all: $(BINS)

$(IMAGEBURN_OBJS) : bindings/server.h marshal.h

image_burner: $(IMAGEBURN_OBJS) image_burner_main.o
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $^ $(LIBS) -o $@

.cc.o:
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -c $< -o $@

bindings/server.h: image_burner.xml
	dbus-binding-tool --mode=glib-server --prefix=$(basename $^) $^ > $@

marshal.h: marshal.list
	glib-genmarshal --header --body --prefix=image_burner $^ > $@

clean:
	rm -rf *.o $(BINDINGS_DIR)/* marshal.h $(BINS)

unittest_runner: unittest_runner.o image_burner_impl_unittest.o $(COMMON_OBJS)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $^ $(LIBS) -lgmock -lgtest -o $@

image_burner_tester.o: bindings/client.h
image_burner_tester: image_burner_tester.o
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $^ $(LIBS) -o $@

bindings/client.h: image_burner.xml
	dbus-binding-tool --mode=glib-client --prefix=$(basename $^) $^ > $@
