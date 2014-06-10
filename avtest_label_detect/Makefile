# Copyright 2014 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

include common.mk

PC_DEPS =

USE_VAAPI ?= $(if $(filter $(ARCH),amd64 x86),yes,no)
ifeq ($(USE_VAAPI),yes)
  CPPFLAGS += -DHAS_VAAPI
  PC_DEPS += libva-x11 x11
  VAAPI_OBJS = util_vaapi.o
endif

PC_CFLAGS := $(shell $(PKG_CONFIG) --cflags $(PC_DEPS))
PC_LIBS := $(shell $(PKG_CONFIG) --libs $(PC_DEPS))

CPPFLAGS += $(PC_CFLAGS)
LDLIBS += $(PC_LIBS)

CC_BINARY(avtest_label_detect): \
	detect_hw_video_acc.o \
	detect_webcam.o \
	util.o \
	util_v4l2.o \
	main.o \
	$(VAAPI_OBJS)

all: CC_BINARY(avtest_label_detect)
