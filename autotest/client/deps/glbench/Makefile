# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

SOURCES_COMMON = xlib_window.cc utils.cc
SOURCES_GL_BENCH = main.cc swaptest.cc yuvtest.cc testbase.cc
SOURCES_GL_BENCH += readpixeltest.cc
SOURCES_GL_BENCH += attributefetchtest.cc varyingsandddxytest.cc cleartest.cc
SOURCES_GL_BENCH += textureupdatetest.cc trianglesetuptest.cc fillratetest.cc
SOURCES_GL_BENCH += windowmanagercompositingtest.cc
SOURCES_GL_BENCH += md5.cc png_helper.cc

# work around USE="opengles" ./run_remote_tests.sh bug and directly parse $USE
ifneq (,$(findstring opengles, $(USE)))
GRAPHICS_BACKEND ?= OPENGLES
endif
GRAPHICS_BACKEND ?= OPENGL

SOURCES_TEARTEST = teartest.cc
SOURCES_WINDOWMANAGERTEST = windowmanagertest.cc
SOURCES_ALL = $(SOURCES_COMMON) \
              $(SOURCES_GL_BENCH) \
              $(SOURCES_TEARTEST) \
              $(SOURCES_WINDOWMANAGERTEST)

PKG_CONFIG ?= pkg-config
BASE_VER = 85268
PC_DEPS = libchrome-$(BASE_VER) libpng
PC_CFLAGS := $(shell $(PKG_CONFIG) --cflags $(PC_DEPS))
PC_LIBS := $(shell $(PKG_CONFIG) --libs $(PC_DEPS))

CXXFLAGS = -g -Wall -Werror
CPPFLAGS += $(PC_CFLAGS)
LDLIBS = $(PC_LIBS) -lgflags
# To compile outside of chroot or with newest libchome, use the following two lines:
#CPPFLAGS += -I$(HOME)/chromium/src -DWORKAROUND_CROSBUG14304
#LDLIBS += -lbase_static -L$(HOME)/chromium/src/out/Release/obj.target/base

GL_BENCH = ../glbench
TEARTEST = ../teartest
WINDOWMANAGERTEST = ../windowmanagertest

ifeq ($(GRAPHICS_BACKEND),OPENGLES)
CPPFLAGS += -DUSE_OPENGLES
SOURCES_COMMON += egl_stuff.cc
SOURCES_TEARTEST += teartest_egl.cc
LDLIBS += -lGLESv2 -lX11 -lEGL
else ifeq ($(GRAPHICS_BACKEND),OPENGL)
CPPFLAGS += -DUSE_OPENGL
SOURCES_COMMON += glx_stuff.cc
SOURCES_TEARTEST += teartest_glx.cc
LDLIBS += -lGL -lX11
endif

OBJS_COMMON = $(SOURCES_COMMON:.cc=.o)
OBJS_GL_BENCH = $(SOURCES_GL_BENCH:.cc=.o)
OBJS_TEARTEST = $(SOURCES_TEARTEST:.cc=.o)
OBJS_WINDOWMANAGERTEST = $(SOURCES_WINDOWMANAGERTEST:.cc=.o)
OBJS_ALL = $(SOURCES_ALL:.cc=.o)
DEPS_ALL = $(SOURCES_ALL:.cc=.d)

.PHONY: all clean

all: $(GL_BENCH) $(TEARTEST) $(WINDOWMANAGERTEST)

$(TEARTEST): $(OBJS_COMMON) $(OBJS_TEARTEST)
$(GL_BENCH): $(OBJS_COMMON) $(OBJS_GL_BENCH)
$(WINDOWMANAGERTEST): $(OBJS_COMMON) $(OBJS_WINDOWMANAGERTEST)

clean:
	$(RM) $(GL_BENCH) $(TEARTEST) $(WINDOWMANAGERTEST)
	$(RM) $(OBJS_ALL) $(DEPS_ALL)
	$(RM) *.o *.d .version

$(GL_BENCH) $(TEARTEST) $(WINDOWMANAGERTEST):
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $^ -o $@ $(LDLIBS)

$(OBJS_ALL): %.o: %.cc
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -c $< -o $@ -MMD

-include $(DEPS_ALL)
