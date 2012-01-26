# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

include $(SRC)/common.mk

define define_helper
all: CXX_BINARY(helpers/$(1))
$(info define_helper: $(1))
CXX_BINARY(helpers/$(1)): helpers/$(1).o
	$$(call cxx_binary,-lbase -lchromeos)
# Please do not delete the newline at the end of define_helper. The text
# produced by define_helper is smashed together by the $(eval) below without any
# separator, so without the newline, we would end up with:
#   $(call cxx_binary ...) all: CXX_BINARY(...)
# which is clearly erroneous.

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
CM_PROXIES := $(patsubst %,$(OUT)proxies/org.chromium.flimflam.%.h, \
                Device IPConfig Manager Profile Service)
DBUSHDRS := $(MM_PROXIES) $(CM_PROXIES) \
            $(OUT)proxies/org.freedesktop.DBus.Properties.h

helpers/modem_status.o.depends: $(DBUSHDRS)
helpers/network_status.o.depends: $(DBUSHDRS)
