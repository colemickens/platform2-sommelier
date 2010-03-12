# Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

SOURCES_COMMON = xlib_window.cc utils.cc
SOURCES_GL_BENCH = main.cc bench.cc shaders.cc
SOURCES_TEARTEST = teartest.cc
SOURCES_ALL = $(SOURCES_COMMON) $(SOURCES_GL_BENCH) $(SOURCES_TEARTEST)

CXXFLAGS = -g -Wall -Werror -I$(GCLIENT_ROOT)/src/third_party/chrome/files
LDFLAGS = -lGL -lX11 -L$(GCLIENT_ROOT)/src/third_party/chrome -lbase -lrt
GL_BENCH = ../glbench
TEARTEST = ../teartest

GCLIENT_ROOT ?= ../../../../../../../..

ifdef USE_EGL
CXXFLAGS += -I$(GCLIENT_ROOT)/src/third_party/khronos/files/include
SOURCES_COMMON += egl_stuff.cc
LDFLAGS += -L$(GCLIENT_ROOT)/src/third_party/khronos/files -lEGL
else
SOURCES_COMMON += glx_stuff.cc
endif

OBJS_COMMON = $(SOURCES_COMMON:.cc=.o)
OBJS_GL_BENCH = $(SOURCES_GL_BENCH:.cc=.o)
OBJS_TEARTEST = $(SOURCES_TEARTEST:.cc=.o)
OBJS_ALL = $(SOURCES_ALL:.cc=.o)
DEPS_ALL = $(SOURCES_ALL:.cc=.d)

.PHONY: all clean

all: $(GL_BENCH) $(TEARTEST)

$(TEARTEST): $(OBJS_COMMON) $(OBJS_TEARTEST)
$(GL_BENCH): $(OBJS_COMMON) $(OBJS_GL_BENCH)

clean:
	$(RM) $(GL_BENCH) $(TEARTEST) $(OBJS_ALL) $(DEPS_ALL)

$(GL_BENCH) $(TEARTEST):
	$(CXX) $^ -o $@ $(CXXFLAGS) $(LDFLAGS)

$(OBJS_ALL): %.o: %.cc
	$(CXX) $(CXXFLAGS) -c $< -o $@ -MMD

-include $(DEPS_ALL)
