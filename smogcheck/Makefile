# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

include common.mk

CPPFLAGS += -I$(SRC)/include

EXEC_NAME = smogcheck
SONAME = libsmogcheck.so

CC_BINARY($(EXEC_NAME)): smogcheck.o CC_LIBRARY($(SONAME))
all: CC_BINARY($(EXEC_NAME))
clean: CLEAN(CC_BINARY($(EXEC_NAME)))

CC_LIBRARY($(SONAME)): lib_smogcheck.o
CC_LIBRARY($(SONAME)): LDFLAGS += -Wl,-soname=$(SONAME)
all: CC_LIBRARY($(SONAME))
clean: CLEAN(CC_LIBRARY($(SONAME)))

install:
	install -D $(OUT)/$(EXEC_NAME) $(DESTDIR)/usr/local/sbin/$(EXEC_NAME)
	install -D -m 0644 $(OUT)/$(SONAME) $(DESTDIR)/usr/lib/$(SONAME)
	ln -s $(SONAME) $(DESTDIR)/usr/lib/$(SONAME).0
