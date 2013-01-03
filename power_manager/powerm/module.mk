# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

include common.mk

LIBPOWERMAN_DEPS = libchromeos-$(BASE_VER)
LIBPOWERMAN_FLAGS = $(GLIB_FLAGS) $(DBUS_FLAGS) \
                    $(shell $(PKG_CONFIG) --cflags $(LIBPOWERMAN_DEPS))
LIBPOWERMAN_LIBS = $(GLIB_LIBS) $(DBUS_LIBS) -lgflags \
                   $(shell $(PKG_CONFIG) --libs $(LIBPOWERMAN_DEPS))
LIBPOWERMAN_OBJS = common/power_constants.o \
                   powerm/powerman.o
CXX_STATIC_LIBRARY(powerm/libpowerman.pie.a): $(LIBPOWERMAN_OBJS)
CXX_STATIC_LIBRARY(powerm/libpowerman.pie.a): CPPFLAGS += $(LIBPOWERMAN_FLAGS)
CXX_STATIC_LIBRARY(powerm/libpowerman.pie.a): LDLIBS += $(LIBPOWERMAN_LIBS)
clean: CLEAN(powerm/libpowerman.pie.a)

POWERMAN_FLAGS = $(LIBPOWERMAN_FLAGS)
POWERMAN_LIBS = $(LIBPOWERMAN_LIBS) -lprotobuf-lite
POWERMAN_OBJS = powerm/powerman_main.o
CXX_BINARY(powerm/powerm): $(POWERMAN_OBJS) \
	CXX_STATIC_LIBRARY(common/libpower_prefs.pie.a) \
	CXX_STATIC_LIBRARY(powerm/libpowerman.pie.a) \
	CXX_STATIC_LIBRARY(common/libutil.pie.a) \
	CXX_STATIC_LIBRARY(common/libutil_dbus.pie.a)
CXX_BINARY(powerm/powerm): CPPFLAGS += $(POWERMAN_FLAGS)
CXX_BINARY(powerm/powerm): LDLIBS += $(POWERMAN_LIBS)
clean: CXX_BINARY(powerm/powerm)
all: CXX_BINARY(powerm/powerm)
