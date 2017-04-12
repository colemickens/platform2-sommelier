# Copyright 2017 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

include common.mk

libsync_OBJS = libsync/src/strlcpy.o libsync/src/sync.o

$(eval $(call add_object_rules,$(libsync_OBJS),CC,c,CFLAGS))

CC_STATIC_LIBRARY(libsync/libsync.pic.a): $(libsync_OBJS)

clean: CLEAN(libsync/libsync.pic.a)

libsync/all: CC_STATIC_LIBRARY(libsync/libsync.pic.a)

.PHONY: libsync/all
