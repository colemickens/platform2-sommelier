# Copyright 2014 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

include common.mk

ifeq ($(ARCH), x86)
	CFLAGS += -DHAS_VAAPI -I$(SYSROOT)/usr/include
	LDLIBS += -L$(SYSROOT)/usr/lib -lva-x11 -lX11 -lva
else
ifeq ($(ARCH), amd64)
	CFLAGS += -DHAS_VAAPI -I$(SYSROOT)/usr/include
	LDLIBS += -L$(SYSROOT)/usr/lib64 -lva-x11 -lX11 -lva
endif
endif

CC_BINARY(avtest_label_detect): \
	detect_hw_video_acc.o \
	detect_webcam.o \
	util.o \
	util_v4l2.o \
	main.o

ifeq ($(ARCH), x86)
CC_BINARY(avtest_label_detect): util_vaapi.o
endif
ifeq ($(ARCH), amd64)
CC_BINARY(avtest_label_detect): util_vaapi.o
endif

all: CC_BINARY(avtest_label_detect)
