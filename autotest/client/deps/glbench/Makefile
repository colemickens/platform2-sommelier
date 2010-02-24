# Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

SOURCES = main.cc bench.cc xlib_window.cc shaders.cc

CXXFLAGS = -g -Wall -Werror -I$(GCLIENT_ROOT)/src/third_party/chrome/files
LDFLAGS = -lGL -lX11 -L$(GCLIENT_ROOT)/src/third_party/chrome -lbase -lrt
TARGET = ../gl_Bench

ifdef USE_EGL
CXXFLAGS += -I$(GCLIENT_ROOT)/src/third_party/khronos/files/include
SOURCES += egl_stuff.cc
LDFLAGS += -L$(GCLIENT_ROOT)/src/third_party/khronos/files -lEGL
else
SOURCES += glx_stuff.cc
endif

OBJS = $(SOURCES:.cc=.o)
DEPS = $(SOURCES:.cc=.d)

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)

clean:
	$(RM) $(TARGET) $(OBJS) $(DEPS)

$(TARGET):
	$(CXX) $^ -o $@ $(CXXFLAGS) $(LDFLAGS)

$(OBJS): %.o: %.cc
	$(CXX) $(CXXFLAGS) -c $< -o $@ -MMD

-include $(DEPS)
