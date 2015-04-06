# Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

include common.mk

CXXFLAGS += \
	-std=gnu++11

CPPFLAGS += \
	-DGESTURES_INTERNAL=1 \
	-I../..

PC_DEPS = libchrome-$(BASE_VER) ncurses x11 xi
PC_CFLAGS := $(shell $(PKG_CONFIG) --cflags $(PC_DEPS))
PC_LIBS := $(shell $(PKG_CONFIG) --libs $(PC_DEPS))

CPPFLAGS += $(PC_CFLAGS)
LDLIBS += $(PC_LIBS)

all: CXX_BINARY(try_touch_experiment)

CXX_BINARY(try_touch_experiment): \
	try_touch_experiment.o \
	salsa_experiment_runner.o \
	treatment.o \
	property.o \
	experiment.o

clean: CLEAN(CXX_BINARY(try_touch_experiment))

EXE = try_touch_experiment
install:
	install -D -m 0755 $(OUT)/$(EXE) $(DESTDIR)/usr/sbin/$(EXE)
