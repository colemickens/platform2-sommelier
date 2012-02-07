# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Stub makefile.

# Make sure a passed in OUT= isn't lost.
OUT ?= $(PWD)/build-opt
$(filter-out _all,$(MAKECMDGOALS)) _all:
	$(MAKE) -C src $(MAKECMDGOALS) OUT=$(OUT)
