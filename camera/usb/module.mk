# Copyright 2016 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

include common.mk

CXX_LIBRARY(usb/libarccamera.so): $(usb_CXX_OBJECTS)

usb/libarccamera: CXX_LIBRARY(usb/libarccamera.so)

clean: CLEAN(usb/libarccamera.so)

.PHONY: usb/libarccamera
