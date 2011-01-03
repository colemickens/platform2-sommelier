# Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

CXX ?= g++
CXXFLAGS ?= -Wall -Werror -g
CXXFLAGS += -DOS_CHROMEOS
PKG_CONFIG ?= pkg-config

BASE_LIBS = -lbase -lpthread -lrt -lchromeos -lbootstat
LIBS = $(BASE_LIBS)
TEST_LIBS = $(BASE_LIBS)
INCLUDE_DIRS = -I.. \
	$(shell $(PKG_CONFIG) --cflags gobject-2.0 dbus-1 dbus-glib-1 nss)
LIB_DIRS = $(shell $(PKG_CONFIG) --libs gobject-2.0 dbus-1 dbus-glib-1 nss)

SESSION_COMMON_OBJS = session_manager_service.o child_job.o interface.o \
	nss_util.o pref_store.o system_utils.o owner_key.o \
	upstart_signal_emitter.o wipe_mitigator.o

DBUS_SOURCE = session_manager.xml
DBUS_SERVER = bindings/server.h
DBUS_CLIENT = bindings/client.h

SESSION_BIN = session_manager
SESSION_OBJS = $(SESSION_COMMON_OBJS) session_manager_main.o

SIGNALLER_BIN = signaller
SIGNALLER_OBJS = signaller.o

TEST_BIN = session_manager_unittest
TEST_OBJS = $(SESSION_COMMON_OBJS) session_manager_testrunner.o \
	session_manager_unittest.o child_job_unittest.o pref_store_unittest.o \
	nss_util_unittest.o owner_key_unittest.o system_utils_unittest.o \
	wipe_mitigator_unittest.o

BINDINGS_DIR = bindings

all: $(SESSION_BIN) $(SIGNALLER_BIN) $(TEST_BIN)

$(SESSION_OBJS) : $(DBUS_SERVER)
$(SIGNALLER_OBJS) : $(DBUS_CLIENT)
$(TEST_OBJS) : $(DBUS_CLIENT)

$(SESSION_BIN): $(SESSION_OBJS)
	$(CXX) $(CXXFLAGS) $(INCLUDE_DIRS) $(LIB_DIRS) $(SESSION_OBJS) \
	$(LIBS) $(LDFLAGS) -o $(SESSION_BIN)

$(SIGNALLER_BIN): $(SIGNALLER_OBJS)
	$(CXX) $(CXXFLAGS) $(INCLUDE_DIRS) $(LIB_DIRS) $(SIGNALLER_OBJS) \
	$(TEST_LIBS) $(LDFLAGS) -o $(SIGNALLER_BIN)

$(TEST_BIN): CXXFLAGS+=-DUNIT_TEST
$(TEST_BIN): $(TEST_OBJS)
	$(CXX) $(CXXFLAGS) $(INCLUDE_DIRS) $(LIB_DIRS) $(TEST_OBJS) \
	$(TEST_LIBS) $(LDFLAGS) -lgtest -lgmock -o $(TEST_BIN)

.cc.o:
	$(CXX) $(CXXFLAGS) $(INCLUDE_DIRS) -c $< -o $@

$(BINDINGS_DIR):
	mkdir -p $(BINDINGS_DIR)

$(DBUS_SERVER): $(DBUS_SOURCE) $(BINDINGS_DIR)
	dbus-binding-tool --mode=glib-server \
	 --prefix=`basename $(DBUS_SOURCE) .xml` $(DBUS_SOURCE) > $(DBUS_SERVER)

$(DBUS_CLIENT): $(DBUS_SOURCE) $(BINDINGS_DIR)
	dbus-binding-tool --mode=glib-client \
	 --prefix=`basename $(DBUS_SOURCE) .xml` $(DBUS_SOURCE) > $(DBUS_CLIENT)

clean:
	rm -rf *.o $(SESSION_BIN) $(SIGNALLER_BIN) $(TEST_BIN) $(BINDINGS_DIR)
