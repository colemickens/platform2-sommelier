# Copyright 2016 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

include common.mk

PC_DEPS = libbrillo-$(BASE_VER) libchrome-$(BASE_VER)
PC_CFLAGS := $(shell SYSROOT=$(SYSROOT) $(PKG_CONFIG) --cflags $(PC_DEPS))
PC_LIBS := $(shell SYSROOT=$(SYSROOT) $(PKG_CONFIG) --libs $(PC_DEPS))

ANDROID_INCLUDE := $(abspath $(SRC)/../android/include)
ANDROID_INCLUDE_PATHS := \
	-I$(ANDROID_INCLUDE) \
	-I$(ANDROID_INCLUDE)/hardware/libhardware/include \
	-I$(ANDROID_INCLUDE)/system/core/include \
	-I$(ANDROID_INCLUDE)/system/core/libsync/include \
	-I$(ANDROID_INCLUDE)/system/media/camera/include \
	-I$(ANDROID_INCLUDE)/system/media/private/camera/include

CPPFLAGS += $(PC_CFLAGS) $(ANDROID_INCLUDE_PATHS) -I$(abspath $(SRC)/../)
CXXFLAGS += -std=c++11 -Wall -Wpointer-arith
CFLAGS += -Wall -std=gnu99

LDLIBS += $(PC_LIBS)

CXX_LIBRARY(libarccamera.so): $(CXX_OBJECTS)

all: CXX_LIBRARY(libarccamera.so)

clean: CLEAN(libarccamera.so)

install: all
	mkdir -p $(DESTDIR)/usr/lib
	install -m 644 $(OUT)/libarccamera.so $(DESTDIR)/usr/lib
