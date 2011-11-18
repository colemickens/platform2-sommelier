# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

include common.mk

# Build a statically linked PIC library
CXX_STATIC_LIBRARY(component/libcomponent.pic.a): component/component.o \
  component/subcomponent/subcomponent.o

# Build a statically linked PIE library using automagically
# created object and sub-path variables.
CXX_STATIC_LIBRARY(component/libcomponent.pie.a): $(component_CXX_OBJECTS) \
  component/subcomponent/subcomponent.o
clean: CLEAN(component/libcomponent.*.a)

# Build a dynamically linked library
CXX_LIBRARY(component/libcomponent.so): component/component.o \
  CC_LIBRARY(component/subcomponent/libsubcomponent.so)
clean: CLEAN(component/libcomponent.so)

