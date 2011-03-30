# Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

CXX ?= g++
CXXFLAGS ?= -Wall -Werror -g
CXXFLAGS += -DOS_CHROMEOS
PKG_CONFIG ?= pkg-config

BASE_LIBS = -lbase -lpthread -lrt -lchromeos -lbootstat -levent -lprotobuf-lite
LIBS = $(BASE_LIBS)
TEST_LIBS = $(BASE_LIBS)
INCLUDE_DIRS = -I.. $(shell $(PKG_CONFIG) --cflags dbus-1 dbus-glib-1 glib-2.0 \
	gdk-2.0 gtk+-2.0 nss)
LIB_DIRS = $(shell $(PKG_CONFIG) --libs dbus-1 dbus-glib-1 glib-2.0 gdk-2.0 \
	gtk+-2.0 nss)


SESSION_COMMON_OBJS = session_manager_service.o device_policy.o child_job.o \
	interface.o nss_util.o owner_key.o pref_store.o system_utils.o \
	upstart_signal_emitter.o wipe_mitigator.o

BINDINGS = bindings

DBUS_SOURCE = session_manager.xml
DBUS_SERVER = $(BINDINGS)/server.h
DBUS_CLIENT = $(BINDINGS)/client.h

PROTO_PATH = $(ROOT)/usr/include/proto
PROTO_DEFS = $(PROTO_PATH)/device_management_backend.proto \
	$(PROTO_PATH)/device_management_local.proto
PROTO_BINDINGS = $(BINDINGS)/device_management_backend.pb.cc \
	$(BINDINGS)/device_management_local.pb.cc
PROTO_OBJS = $(BINDINGS)/device_management_backend.pb.o \
	$(BINDINGS)/device_management_local.pb.o

COMMON_OBJS = session_manager_service.o device_policy.o child_job.o \
	interface.o nss_util.o owner_key.o pref_store.o system_utils.o \
	upstart_signal_emitter.o wipe_mitigator.o $(PROTO_OBJS)

KEYGEN_BIN = keygen
KEYGEN_OBJS = keygen.o nss_util.o owner_key.o system_utils.o

SESSION_BIN = session_manager
SESSION_OBJS = $(COMMON_OBJS) session_manager_main.o

TEST_BIN = session_manager_unittest
TEST_OBJS = $(COMMON_OBJS) session_manager_testrunner.o \
	session_manager_unittest.o child_job_unittest.o \
	device_policy_unittest.o pref_store_unittest.o \
	nss_util_unittest.o owner_key_unittest.o system_utils_unittest.o \
	wipe_mitigator_unittest.o

BINDINGS = bindings

all: $(KEYGEN_BIN) $(SESSION_BIN) $(TEST_BIN)

$(COMMON_OBJS) : $(PROTO_BINDINGS)
$(SESSION_OBJS) : $(DBUS_SERVER)
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

$(BINDINGS):
	mkdir -p $(BINDINGS)

$(DBUS_SERVER): $(BINDINGS) $(DBUS_SOURCE)
	dbus-binding-tool --mode=glib-server \
	 --prefix=`basename $(DBUS_SOURCE) .xml` $(DBUS_SOURCE) > $(DBUS_SERVER)

$(DBUS_CLIENT): $(BINDINGS) $(DBUS_SOURCE)
	dbus-binding-tool --mode=glib-client \
	 --prefix=`basename $(DBUS_SOURCE) .xml` $(DBUS_SOURCE) > $(DBUS_CLIENT)

$(PROTO_BINDINGS): $(BINDINGS) $(PROTO_DEFS)
	protoc --proto_path=$(PROTO_PATH) --cpp_out=$(BINDINGS) $(PROTO_DEFS)

clean:
	rm -rf *.o $(KEYGEN_BIN) $(SESSION_BIN) $(TEST_BIN) $(BINDINGS)
