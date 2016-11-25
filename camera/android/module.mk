# Copyright 2016 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

include common.mk

ANDROID_INCLUDE := $(abspath $(SRC)/android/include)
ANDROID_INCLUDE_PATHS := \
	-I$(ANDROID_INCLUDE) \
	-I$(ANDROID_INCLUDE)/hardware/libhardware/include \
	-I$(ANDROID_INCLUDE)/system/core/include \
	-I$(ANDROID_INCLUDE)/system/core/libsync/include \
	-I$(ANDROID_INCLUDE)/system/media/camera/include \
	-I$(ANDROID_INCLUDE)/system/media/private/camera/include

ANDROID_SRC := $(abspath $(SRC)/android/src)
ANDROID_C_OBJECTS := \
	$(ANDROID_SRC)/camera_metadata.o \
	$(ANDROID_SRC)/native_handle.o \
	$(ANDROID_SRC)/sync.o

$(eval $(call add_object_rules,$(ANDROID_C_OBJECTS),CC,c,CFLAGS))

clean: CLEAN($(ANDROID_C_OBJECTS))

# To link against object files under android/, add $(ANDROID_OBJECTS) to the
# dependency list of your target.
ANDROID_OBJECTS := $(ANDROID_C_OBJECTS)
