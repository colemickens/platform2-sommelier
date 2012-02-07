# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

include $(SRC)/common.mk

define define_helper
all: CXX_BINARY($(basename $(1)))
$(info define_helper: $(1))
$(1).depends: $$(DBUSHDRS)
CXX_BINARY($(basename $(1))): LDFLAGS += -lbase -lchromeos
CXX_BINARY($(basename $(1))): $(1)
# Please do not delete the newline at the end of define_helper. The text
# produced by define_helper is smashed together by the $(eval) below without any
# separator, so without the newline, we would end up with:
#   $(call cxx_binary ...) all: CXX_BINARY(...)
# which is clearly erroneous.

endef

MM_PROXIES := $(patsubst %,proxies/org.freedesktop.ModemManager%,.h \
                .Modem.h .Modem.Simple.h)
CM_PROXIES := $(patsubst %,proxies/org.chromium.flimflam.%.h, \
                Device IPConfig Manager Profile Service)
DBUSHDRS := $(MM_PROXIES) $(CM_PROXIES) \
            proxies/org.freedesktop.DBus.Properties.h

# Construct build rules for all of the helpers. For each helper h, define a rule
# for CXX_BINARY(helpers/h), and make CXX_BINARY(helpers/h) a dependency of
# 'all'.
$(eval \
    $(foreach helper, \
              $(helpers_CXX_OBJECTS), \
              $(call define_helper,$(helper))))
