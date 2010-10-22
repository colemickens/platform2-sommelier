# Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

SOURCES_COMMON = xlib_window.cc utils.cc
SOURCES_GL_BENCH = main.cc swaptest.cc yuvtest.cc testbase.cc
SOURCES_GL_BENCH += readpixeltest.cc
SOURCES_GL_BENCH += attributefetchtest.cc varyingsandddxytest.cc cleartest.cc
SOURCES_GL_BENCH += textureupdatetest.cc trianglesetuptest.cc fillratetest.cc

GRAPHICS_BACKEND ?= OPENGL

# TODO: port these to GLES
ifeq ($(GRAPHICS_BACKEND),OPENGL)
SOURCES_GL_BENCH += windowmanagercompositingtest.cc
endif
SOURCES_TEARTEST = teartest.cc
SOURCES_WINDOWMANAGERTEST = windowmanagertest.cc
SOURCES_ALL = $(SOURCES_COMMON) \
              $(SOURCES_GL_BENCH) \
              $(SOURCES_TEARTEST) \
              $(SOURCES_WINDOWMANAGERTEST)

CXXFLAGS = -g -Wall -Werror
LDFLAGS = -lbase -lrt -lgflags -lglib-2.0 -pthread
# To compile outside of chroot, use the following two lines:
#CXXFLAGS += -I../../../../../../chrome/files
#LDFLAGS += -L../../../../../../chrome

GL_BENCH = ../glbench
TEARTEST = ../teartest
WINDOWMANAGERTEST = ../windowmanagertest

ifeq ($(GRAPHICS_BACKEND),OPENGLES)
CXXFLAGS += -DUSE_OPENGLES
SOURCES_COMMON += egl_stuff.cc
SOURCES_TEARTEST += teartest_egl.cc
LDFLAGS += -lGLESv2 -lX11 -lEGL
else ifeq ($(GRAPHICS_BACKEND),OPENGL)
CXXFLAGS += -DUSE_OPENGL -DI915_WORKAROUND
SOURCES_COMMON += glx_stuff.cc
SOURCES_TEARTEST += teartest_glx.cc
LDFLAGS += -lGL -lX11
endif

OBJS_COMMON = $(SOURCES_COMMON:.cc=.o)
OBJS_GL_BENCH = $(SOURCES_GL_BENCH:.cc=.o)
OBJS_TEARTEST = $(SOURCES_TEARTEST:.cc=.o)
OBJS_WINDOWMANAGERTEST = $(SOURCES_WINDOWMANAGERTEST:.cc=.o)
OBJS_ALL = $(SOURCES_ALL:.cc=.o)
DEPS_ALL = $(SOURCES_ALL:.cc=.d)

.PHONY: all clean

all: $(GL_BENCH) $(TEARTEST)

ifeq ($(GRAPHICS_BACKEND),OPENGL)
all: $(WINDOWMANAGERTEST)
endif

$(TEARTEST): $(OBJS_COMMON) $(OBJS_TEARTEST)
$(GL_BENCH): $(OBJS_COMMON) $(OBJS_GL_BENCH)
$(WINDOWMANAGERTEST): $(OBJS_COMMON) $(OBJS_WINDOWMANAGERTEST)

clean:
	$(RM) $(GL_BENCH) $(TEARTEST) $(WINDOWMANAGERTEST)
	$(RM) $(OBJS_ALL) $(DEPS_ALL)
	$(RM) *.o *.d

$(GL_BENCH) $(TEARTEST) $(WINDOWMANAGERTEST):
	$(CXX) $^ -o $@ $(CXXFLAGS) $(LDFLAGS)

$(OBJS_ALL): %.o: %.cc
	$(CXX) $(CXXFLAGS) -c $< -o $@ -MMD

-include $(DEPS_ALL)
