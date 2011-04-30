# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

CXX ?= g++
CXXFLAGS ?= -Wall -Werror -g
CXXFLAGS += -DOS_CHROMEOS
PKG_CONFIG ?= pkg-config

BASE_LIBS = -lpthread -lrt -lchromeos -lbootstat -levent -lprotobuf-lite -lbase
LIBS = $(BASE_LIBS)
TEST_LIBS = $(BASE_LIBS)
INCLUDE_DIRS = -I.. $(shell $(PKG_CONFIG) --cflags dbus-1 dbus-glib-1 glib-2.0 \
	gdk-2.0 gtk+-2.0 nss)
LIB_DIRS = $(shell $(PKG_CONFIG) --libs dbus-1 dbus-glib-1 glib-2.0 gdk-2.0 \
	gtk+-2.0 nss)

BINDINGS = bindings

DBUS_SOURCE = session_manager.xml
DBUS_SERVER = $(BINDINGS)/server.h
DBUS_CLIENT = $(BINDINGS)/client.h

PROTO_PATH = $(ROOT)/usr/include/proto
PROTO_DEFS = $(PROTO_PATH)/chrome_device_policy.proto \
	$(PROTO_PATH)/device_management_backend.proto
PROTO_BINDINGS = $(BINDINGS)/chrome_device_policy.pb.cc \
	$(BINDINGS)/device_management_backend.pb.cc
PROTO_HEADERS = $(patsubst %.cc,%.h,$(PROTO_BINDINGS))
PROTO_OBJS = $(BINDINGS)/chrome_device_policy.pb.o \
	$(BINDINGS)/device_management_backend.pb.o

COMMON_OBJS = child_job.o device_policy.o interface.o key_generator.o \
	nss_util.o owner_key.o owner_key_loss_mitigator.o regen_mitigator.o \
	session_manager_service.o system_utils.o upstart_signal_emitter.o \
	$(PROTO_OBJS)

KEYGEN_BIN = keygen
KEYGEN_OBJS = keygen.o nss_util.o owner_key.o system_utils.o

SESSION_BIN = session_manager
SESSION_OBJS = session_manager_main.o

TEST_BIN = session_manager_unittest
TEST_OBJS = session_manager_testrunner.o child_job_unittest.o \
	device_policy_unittest.o key_generator_unittest.o \
	mock_constructors.o mock_nss_util.o nss_util_unittest.o \
	owner_key_unittest.o regen_mitigator_unittest.o \
	session_manager_unittest.o session_manager_static_unittest.o \
	system_utils_unittest.o

all: $(KEYGEN_BIN) $(SESSION_BIN) $(TEST_BIN)

$(PROTO_HEADERS): %.h: %.cc

$(COMMON_OBJS): $(PROTO_HEADERS)
$(SESSION_OBJS): $(DBUS_SERVER) $(COMMON_OBJS)
$(TEST_OBJS): $(DBUS_SERVER) $(COMMON_OBJS)

$(KEYGEN_BIN): $(KEYGEN_OBJS)
	$(CXX) $(CXXFLAGS) $(INCLUDE_DIRS) $(LIB_DIRS) $(KEYGEN_OBJS) \
	$(LIBS) $(LDFLAGS) -o $(KEYGEN_BIN)

$(SESSION_BIN): $(SESSION_OBJS)
	$(CXX) $(CXXFLAGS) $(INCLUDE_DIRS) $(LIB_DIRS) $(SESSION_OBJS) \
	$(COMMON_OBJS) $(NEED_PROTO_OBJS) $(LIBS) $(LDFLAGS) -o $(SESSION_BIN)

$(TEST_BIN): CXXFLAGS+=-DUNIT_TEST
$(TEST_BIN): $(TEST_OBJS)
	$(CXX) $(CXXFLAGS) $(INCLUDE_DIRS) $(LIB_DIRS) $(TEST_OBJS) \
	$(COMMON_OBJS) $(NEED_PROTO_OBJS) $(TEST_LIBS) $(LDFLAGS) -lgtest \
	-lgmock -o $(TEST_BIN)

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
