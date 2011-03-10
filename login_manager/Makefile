# Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

CXX ?= g++
CXXFLAGS ?= -Wall -Werror -g
CXXFLAGS += -DOS_CHROMEOS
PKG_CONFIG ?= pkg-config

BASE_LIBS = -lbase -lpthread -lrt -lchromeos -lbootstat -levent
LIBS = $(BASE_LIBS)
TEST_LIBS = $(BASE_LIBS)
INCLUDE_DIRS = -I.. $(shell $(PKG_CONFIG) --cflags dbus-1 dbus-glib-1 glib-2.0 \
	gdk-2.0 gtk+-2.0 nss)
LIB_DIRS = $(shell $(PKG_CONFIG) --libs dbus-1 dbus-glib-1 glib-2.0 gdk-2.0 \
	gtk+-2.0 nss)

SESSION_COMMON_OBJS = session_manager_service.o device_policy.o child_job.o \
	interface.o nss_util.o owner_key.o pref_store.o system_utils.o \
	upstart_signal_emitter.o wipe_mitigator.o

DBUS_SOURCE = session_manager.xml
DBUS_SERVER = bindings/server.h
DBUS_CLIENT = bindings/client.h

KEYGEN_BIN = keygen
KEYGEN_OBJS = keygen.o nss_util.o owner_key.o system_utils.o

SESSION_BIN = session_manager
SESSION_OBJS = $(SESSION_COMMON_OBJS) session_manager_main.o

TEST_BIN = session_manager_unittest
TEST_OBJS = $(SESSION_COMMON_OBJS) session_manager_testrunner.o \
	session_manager_unittest.o child_job_unittest.o \
	device_policy_unittest.o pref_store_unittest.o \
	nss_util_unittest.o owner_key_unittest.o system_utils_unittest.o \
	wipe_mitigator_unittest.o

BINDINGS_DIR = bindings

all: $(KEYGEN_BIN) $(SESSION_BIN) $(TEST_BIN)

$(SESSION_OBJS) : $(DBUS_SERVER)
$(SIGNALLER_OBJS) : $(DBUS_CLIENT)
$(TEST_OBJS) : $(DBUS_CLIENT)

$(KEYGEN_BIN): $(KEYGEN_OBJS)
	$(CXX) $(CXXFLAGS) $(INCLUDE_DIRS) $(LIB_DIRS) $(KEYGEN_OBJS) \
	$(LIBS) $(LDFLAGS) -o $(KEYGEN_BIN)

$(SESSION_BIN): $(SESSION_OBJS)
	$(CXX) $(CXXFLAGS) $(INCLUDE_DIRS) $(LIB_DIRS) $(SESSION_OBJS) \
	$(LIBS) $(LDFLAGS) -o $(SESSION_BIN)

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
	rm -rf *.o $(KEYGEN_BIN) $(SESSION_BIN) $(TEST_BIN) $(BINDINGS_DIR)
