# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

include $(SRC)/common.mk

define define_helper
all: CXX_BINARY(helpers/$(1))
$(info define_helper: $(1))
CXX_BINARY(helpers/$(1)): helpers/$(1).o
	$$(call cxx_binary,-lbase -lchromeos)
endef

# Construct build rules for all of the helpers. For each helper h, define a rule
# for CXX_BINARY(helpers/h), and make CXX_BINARY(helpers/h) a dependency of
# 'all'.
$(eval \
    $(foreach helper, \
              $(wildcard $(SRC)/helpers/*.cc), \
              $(call define_helper,$(notdir $(basename $(helper))))))

MM_PROXIES := $(patsubst %,$(OUT)proxies/org.freedesktop.ModemManager%,.h \
                .Modem.h .Modem.Simple.h)

helpers/modem_status.o.depends: $(MM_PROXIES) \
                                $(OUT)proxies/org.freedesktop.DBus.Properties.h
