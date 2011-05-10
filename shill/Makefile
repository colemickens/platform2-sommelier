# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

CXX ?= g++
CXXFLAGS ?= -fno-strict-aliasing -Wall -Wextra -Werror -Wuninitialized
CPPFLAGS ?= -D__STDC_FORMAT_MACROS -D__STDC_LIMIT_MACROS
AR ?= ar
PKG_CONFIG ?= pkg-config
DBUSXX_XML2CPP = dbusxx-xml2cpp


BASE_LIBS = -lbase -lchromeos -lpthread -lrt
BASE_INCLUDE_DIRS = -I..
BASE_LIB_DIRS =

LIBS = $(BASE_LIBS)
INCLUDE_DIRS = $(BASE_INCLUDE_DIRS) $(shell $(PKG_CONFIG) --cflags glib-2.0)
LIB_DIRS = $(BASE_LIB_DIRS) $(shell $(PKG_CONFIG) --libs glib-2.0)

TEST_LIBS = $(BASE_LIBS) -lgmock -lgtest
TEST_INCLUDE_DIRS = $(INCLUDE_DIRS)
TEST_LIB_DIRS = $(LIB_DIRS)

XMLFILES = flimflam-device.xml \
	flimflam-manager.xml \
	flimflam-service.xml \
	supplicant_cli.xml

DBUS_HEADERS = $(patsubst %.xml,%.h,$(XMLFILES))

SHILL_LIB = shill_lib.a
SHILL_OBJS = \
	dbus_control.o \
	device.o \
	manager.o \
	resource.o \
	service.o \
	shill_config.o \
	shill_daemon.o \
	shill_event.o

SHILL_BIN = shill
SHILL_MAIN_OBJ = shill_main.o

TEST_BIN = shill_unittest
TEST_OBJS = testrunner.o shill_unittest.o

all: $(SHILL_BIN) $(TEST_BIN)

%_cli.h: %_cli.xml
	$(DBUSXX_XML2CPP) $< --proxy=$@

%.h: %.xml
	$(DBUSXX_XML2CPP) $< --adaptor=$@

.cc.o:
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(INCLUDE_DIRS) -c $< -o $@

$(SHILL_OBJS): $(DBUS_HEADERS)

$(SHILL_LIB): $(SHILL_OBJS)
	$(AR) rcs $@ $^

$(SHILL_BIN): $(SHILL_MAIN_OBJ) $(SHILL_LIB)
	$(CXX) $(CXXFLAGS) $(INCLUDE_DIRS) $(LIB_DIRS) $(LDFLAGS) $^ $(LIBS) \
	-o $@

$(TEST_BIN): CXXFLAGS += -DUNIT_TEST
$(TEST_BIN): $(TEST_OBJS) $(SHILL_LIB)
	$(CXX) $(CXXFLAGS) $(TEST_INCLUDE_DIRS) $(TEST_LIB_DIRS) $(LDFLAGS) $^ \
		$(TEST_LIBS) -o $@

clean:
	rm -rf *.o $(DBUS_HEADERS) $(SHILL_BIN) $(SHILL_LIB) $(TEST_BIN)
