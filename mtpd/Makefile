# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

include common.mk

BASE_VER ?= 125070
PC_DEPS = dbus-c++-1 glib-2.0 gthread-2.0 gobject-2.0 \
	libchrome-$(BASE_VER) libchromeos-$(BASE_VER)
PC_CFLAGS := $(shell $(PKG_CONFIG) --cflags $(PC_DEPS))
PC_LIBS := $(shell $(PKG_CONFIG) --libs $(PC_DEPS))

CPPFLAGS += -I$(SRC)/include -I$(SRC)/.. -I$(SRC) -I$(OUT) \
	$(PC_CFLAGS)
LDLIBS += -lgflags $(PC_LIBS)

CXX_BINARY(mtpd): $(filter-out %_testrunner.o %_unittest.o,$(CXX_OBJECTS))
clean: CLEAN(mtpd)

# Some shortcuts
mtpd: CXX_BINARY(mtpd)
all: mtpd
