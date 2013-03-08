# Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

OBJDIR = obj

OBJECTS=\
	$(OBJDIR)/try_touch_experiment.o \
	$(OBJDIR)/salsa_experiment_runner.o \
	$(OBJDIR)/treatment.o \
	$(OBJDIR)/property.o \
	$(OBJDIR)/experiment.o

DESTDIR = .

CXXFLAGS+=\
	-g \
	-fno-exceptions \
	-fno-strict-aliasing \
	-Wall \
	-Wclobbered \
	-Wempty-body \
	-Werror \
	-Wignored-qualifiers \
	-Wmissing-field-initializers \
	-Wsign-compare \
	-Wtype-limits \
	-Wuninitialized
CPPFLAGS+=\
	-D__STDC_FORMAT_MACROS=1 \
	-D_LARGEFILE_SOURCE \
	-D_FILE_OFFSET_BITS=64 \
	-DGESTURES_INTERNAL=1 \
	-I..

PKG_CONFIG ?= pkg-config
BASE_VER ?= 180609
PC_DEPS = libchrome-$(BASE_VER)
PC_CFLAGS := $(shell $(PKG_CONFIG) --cflags $(PC_DEPS))
PC_LIBS := $(shell $(PKG_CONFIG) --libs $(PC_DEPS))

CPPFLAGS += $(PC_CFLAGS)

LDLIBS=\
	$(PC_LIBS) \
	-lcurses

define auto_mkdir
  $(if $(wildcard $(dir $1)),$2,$(QUIET)mkdir -p "$(dir $1)")
endef

EXE=try_touch_experiment

all: $(EXE)

$(EXE): $(OBJECTS)
	$(CXX) -o $@ $(CXXFLAGS) $(LDFLAGS) $(OBJECTS) $(LDLIBS)

$(OBJDIR)/%.o : %.cc
	$(call auto_mkdir,$@)
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -c -o $@ $<

$(OBJDIR)/%.o : %.c
	$(call auto_mkdir,$@)
	$(CC) $(CFLAGS) $(CPPFLAGS) -c -o $@ $<

install: $(EXE)
	install -D -m 0755 $(EXE) $(DESTDIR)/usr/sbin/$(EXE)

clean:
	rm -rf $(OBJDIR) $(EXE)

.PHONY : clean all
