# Copyright 2016 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# A function to generate the pkg-config cflags.
define get_pc_cflags
$(shell SYSROOT=$(SYSROOT) $(PKG_CONFIG) --cflags $1)
endef

# A function to generate the pkg-config libs.
define get_pc_libs
$(shell SYSROOT=$(SYSROOT) $(PKG_CONFIG) --libs $1)
endef
