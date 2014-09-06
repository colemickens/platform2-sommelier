# Copyright 2014 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

include common.mk

PC_DEPS =

USE_VAAPI ?= $(if $(filter $(ARCH),amd64 x86),yes,no)
ifeq ($(USE_VAAPI),yes)
  CPPFLAGS += -DHAS_VAAPI
  PC_DEPS += libva-x11 x11
  VAAPI_OBJS = $(filter util_vaapi.o,$(C_OBJECTS))
endif

ifneq ($(PC_DEPS),)
PC_CFLAGS := $(shell $(PKG_CONFIG) --cflags $(PC_DEPS))
PC_LIBS := $(shell $(PKG_CONFIG) --libs $(PC_DEPS))
endif

CPPFLAGS += $(PC_CFLAGS)
LDLIBS += $(PC_LIBS)

CC_BINARY(avtest_label_detect): \
  $(filter-out util_vaapi.o,$(C_OBJECTS)) \
  $(VAAPI_OBJS)

all: CC_BINARY(avtest_label_detect)
clean: CLEAN(CC_BINARY(avtest_label_detect))
