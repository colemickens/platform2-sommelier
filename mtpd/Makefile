# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

include common.mk

DBUSXX_XML2CPP = dbusxx-xml2cpp

# Create a generator for DBUS-C++ headers.
# TODO(wad): integrate into common.mk.
GEN_DBUSXX(%):
	$(call check_deps)
	$(call old_or_no_timestamp,\
		mkdir -p "$(dir $(TARGET_OR_MEMBER))" && \
		$(DBUSXX_XML2CPP) $< --adaptor=$(TARGET_OR_MEMBER))

BASE_VER ?= 125070
PC_DEPS = dbus-c++-1 glib-2.0 gthread-2.0 gobject-2.0 libmtp \
	libchrome-$(BASE_VER) libchromeos-$(BASE_VER) protobuf
PC_CFLAGS := $(shell $(PKG_CONFIG) --cflags $(PC_DEPS))
PC_LIBS := $(shell $(PKG_CONFIG) --libs $(PC_DEPS))

CPPFLAGS += -I$(SRC)/include -I$(SRC)/.. -I$(SRC) -I$(OUT) \
	-I$(OUT)mtpd_server $(PC_CFLAGS)
LDLIBS += -lgflags -ludev $(PC_LIBS)

GEN_DBUSXX(mtpd_server/mtpd_server.h): $(SRC)/mtpd.xml
mtpd_server/mtpd_server.h: GEN_DBUSXX(mtpd_server/mtpd_server.h)
clean: CLEAN(mtpd_server/mtpd_server.h)

# Require the header to be generated first.
$(patsubst %.o,%.o.depends,$(CXX_OBJECTS)): mtpd_server/mtpd_server.h

CXX_BINARY(mtpd): $(filter-out %_testrunner.o %_unittest.o,$(CXX_OBJECTS))
clean: CLEAN(mtpd)

# Some shortcuts
mtpd: CXX_BINARY(mtpd)
all: mtpd
