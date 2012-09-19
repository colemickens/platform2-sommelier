# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This Makefile uses ChromeOS's common.mk for most of its functionality. Consult
# the documentation in common-mk for recipes and other information.

# This controls where we build the package. Don't fiddle with this, since things
# like CPPFLAGS and some of the protobuf stuff depends on it.
OUT ?= $(PWD)/out
VERBOSE ?= 0

include common.mk

PROTOC ?= protoc
# BASE_VER is used to determine what versions of libbase and libchromeos we are
# going to compile against
BASE_VER ?= 125070
PKG_CONFIG ?= pkg-config

# base_env
CPPFLAGS += -D__STDC_FORMAT_MACROS -D__STDC_LIMIT_MACROS -I$(SRC)/.. -I$(OUT)
CPPFLAGS-$(USE_NEW_POWER_BUTTON) += -DNEW_POWER_BUTTON
CPPFLAGS-$(USE_LOCKVT) += -DSUSPEND_LOCK_VT
CPPFLAGS-$(USE_IS_DESKTOP) += -DIS_DESKTOP
CPPFLAGS-$(USE_ALS) += -DHAS_ALS
CPPFLAGS-$(USE_HAS_KEYBOARD_BACKLIGHT) += -DHAS_KEYBOARD_BACKLIGHT
CPPFLAGS-$(USE_AURA) += -DUSE_AURA
CPPFLAGS-$(USE_STAY_AWAKE_WITH_HEADPHONES) += \
	-DSTAY_AWAKE_PLUGGED_DEVICE='"HDA Intel PCH: Cirrus Analog"'
CPPFLAGS-$(USE_TOUCH_DEVICE) += -DTOUCH_DEVICE
CPPFLAGS += $(CPPFLAGS-y)

# shared variables
DBUS_DEPS := dbus-1 dbus-glib-1
DBUS_FLAGS := $(shell $(PKG_CONFIG) --cflags $(DBUS_DEPS))
DBUS_LIBS := $(shell $(PKG_CONFIG) --libs $(DBUS_DEPS))

GLIB_DEPS := glib-2.0 gobject-2.0 libchrome-$(BASE_VER) libchromeos-$(BASE_VER)
GLIB_FLAGS := $(shell $(PKG_CONFIG) --cflags $(GLIB_DEPS))
GLIB_LIBS := $(shell $(PKG_CONFIG) --libs $(GLIB_DEPS))

## protobuffer targets
# Detailed instructions on how to work with these rules can be found in the
# common-mk package.  These rules are for including protobufs that live in the
# dbus directory of the system_api repo. To add a new one from this location to
# the build, just add the name of the generated .cc file to the bindings
# variable and include approriate depends targets[.
#
# To add protobufs from a different location then you need to copy all the proto
# variables and use a new prefix for them, setting the path variable as you
# need.
#
# TODO(rharrison:crosbug.com/30056): Once I have written the protobuf macros,
#                                    use them here
SYSTEM_API_PROTO_BINDINGS = power_state_control.pb.cc \
                            power_supply_properties.pb.cc \
                            video_activity_update.pb.cc
SYSTEM_API_PROTO_PATH = $(SYSROOT)/usr/include/chromeos/dbus
SYSTEM_API_PROTO_HEADERS = $(patsubst %.cc,%.h,$(SYSTEM_API_PROTO_BINDINGS))
SYSTEM_API_PROTO_OBJS = $(patsubst %.cc,%.o,$(SYSTEM_API_PROTO_BINDINGS))
$(SYSTEM_API_PROTO_HEADERS): %.h: %.cc ;
$(SYSTEM_API_PROTO_BINDINGS): %.pb.cc: $(SYSTEM_API_PROTO_PATH)/%.proto
	$(PROTOC) --proto_path=$(SYSTEM_API_PROTO_PATH) --cpp_out=. $<
clean: CLEAN($(SYSTEM_API_PROTO_BINDINGS))
clean: CLEAN($(SYSTEM_API_PROTO_HEADERS))
clean: CLEAN($(SYSTEM_API_PROTO_OBJS))
# Add rules for compiling generated protobuffer code, as the CXX_OBJECTS list
# is built before these source files exists and, as such, does not contain them.
$(eval $(call add_object_rules,$(SYSTEM_API_PROTO_OBJS),CXX,cc))
