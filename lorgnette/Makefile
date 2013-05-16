# Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

AR ?= ar
CC ?= gcc
CXX ?= g++
CFLAGS += \
	-Wall \
	-Werror \
	-Wextra \
	-Wno-missing-field-initializers \
	-Wno-unused-parameter \
	-Wno-unused-result \
	-Wuninitialized
CXXFLAGS ?= -fno-strict-aliasing

# -std=gnu++11 is c++11 with GNU extensions. In particular, typeof is allowed.
CXXFLAGS += $(CFLAGS) \
	-std=gnu++11 \
	-Woverloaded-virtual
CXXFLAGS += $(EXTRA_CXXFLAGS)
CPPFLAGS ?= -D__STDC_FORMAT_MACROS -D__STDC_LIMIT_MACROS
PKG_CONFIG ?= pkg-config
DBUSXX_XML2CPP = dbusxx-xml2cpp

BUILDDIR = build
VPATH = $(BUILDDIR)

LIBDIR = /usr/lib
RUNDIR = /var/run/lorgnette
CPPFLAGS += -DRUNDIR=\"$(RUNDIR)\"

BASE_VER ?= 180609
COMMON_PC_DEPS = libchrome-$(BASE_VER) libchromeos-$(BASE_VER)
LORGNETTE_PC_DEPS = \
	$(COMMON_PC_DEPS) \
	dbus-c++-1 \
	gio-2.0 \
	glib-2.0
INCLUDE_DIRS = \
	-iquote.. \
	-iquote $(BUILDDIR) \
	$(shell $(PKG_CONFIG) --cflags $(LORGNETTE_PC_DEPS))
LORGNETTE_LIBS = \
	-lmetrics \
	-lminijail \
	-lrt \
	$(shell $(PKG_CONFIG) --libs $(LORGNETTE_PC_DEPS))

DBUS_BINDINGS_DIR = dbus_bindings
BUILD_DBUS_BINDINGS_DIR = $(BUILDDIR)/lorgnette/$(DBUS_BINDINGS_DIR)

_CREATE_BUILDDIR := $(shell mkdir -p \
	$(BUILDDIR) \
	$(BUILD_DBUS_BINDINGS_DIR))

# Rename local XML files with the names required by DBus to XML files with the
# names required by the style guide, which will then be turned into generated
# headers later.
DBUS_BINDINGS_XML_LOCAL = \
	org.chromium.lorgnette.Manager>lorgnette-manager

define ADD_LOCAL_BINDING
$(eval _SOURCE = $(word 1,$(subst >, ,$(1))))
$(eval _TARGET = $(word 2,$(subst >, ,$(1))))
DBUS_ADAPTOR_HEADERS += $(_TARGET).h
$(BUILD_DBUS_BINDINGS_DIR)/$(_TARGET).xml: $(DBUS_BINDINGS_DIR)/$(_SOURCE).xml
	echo "  CP " $$@
	cp $$< $$@
endef

$(foreach b,$(DBUS_BINDINGS_XML_LOCAL),$(eval $(call ADD_LOCAL_BINDING,$(b))))

DBUS_ADAPTOR_BINDINGS = \
	$(addprefix $(BUILD_DBUS_BINDINGS_DIR)/, $(DBUS_ADAPTOR_HEADERS))

LORGNETTE_LIB = $(BUILDDIR)/lorgnette.a
LORGNETTE_OBJS = $(addprefix $(BUILDDIR)/, daemon.o manager.o minijail.o)

LORGNETTE_BIN = lorgnette
# Broken out separately, because (unlike other LORGNETTE_OBJS), it
# shouldn't be linked into TEST_BIN.
LORGNETTE_MAIN_OBJ = $(BUILDDIR)/main.o

OBJS = $(LORGNETTE_MAIN_OBJ) $(LORGNETTE_OBJS)

.PHONY: all clean

all: $(LORGNETTE_BIN)

$(BUILD_DBUS_BINDINGS_DIR)/%.xml: $(DBUS_BINDINGS_DIR)/%.xml
	echo "  CP " $@
	cp $< $@

$(DBUS_ADAPTOR_BINDINGS): %.h: %.xml
	echo "  DBUSXX_XML2CPP " $@
	$(DBUSXX_XML2CPP) $< --adaptor=$@

$(BUILDDIR)/%.o: %.cc
	echo "  CXX " $@
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(INCLUDE_DIRS) -MMD -c $< -o $@

$(LORGNETTE_OBJS): $(DBUS_ADAPTOR_BINDINGS)

$(LORGNETTE_LIB): $(LORGNETTE_OBJS)
	echo "  AR " $@
	$(AR) rcs $@ $^

$(LORGNETTE_BIN): $(LORGNETTE_MAIN_OBJ) $(LORGNETTE_LIB)
	echo "  CXX " $@
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $^ $(LORGNETTE_LIBS) -o $@

clean:
	rm -rf \
		$(BUILDDIR) \
		$(LORGNETTE_BIN)

# Don't output build commands. When adding rules, please echo a short
# form of the build command manually. E.g. 'echo "  CXX " $@'.
.SILENT:

-include $(OBJS:.o=.d)
