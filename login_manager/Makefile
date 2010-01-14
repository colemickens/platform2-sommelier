# Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

CXX=g++
LDFLAGS=-lbase -lpthread -lrt -lchromeos
CXXFLAGS=-Wall -Werror -g
INCLUDE_DIRS=-I../../third_party/chrome/files -I.. $(shell pkg-config --cflags gobject-2.0 dbus-1 dbus-glib-1)
LIB_DIRS=-L../../third_party/chrome $(shell pkg-config --libs gobject-2.0 dbus-1 dbus-glib-1)

SESSION_COMMON_OBJS=session_manager_service.o child_job.o interface.o constants.o

DBUS_SOURCE=session_manager.xml
DBUS_SERVER=bindings/server.h
DBUS_CLIENT=bindings/client.h

SESSION_BIN=session_manager
SESSION_OBJS=$(SESSION_COMMON_OBJS) session_manager_main.o

SIGNALLER_BIN=signaller
SIGNALLER_OBJS=signaller.o constants.o

TEST_BIN=session_manager_unittest
TEST_OBJS=$(SESSION_COMMON_OBJS) session_manager_testrunner.o \
	session_manager_unittest.o child_job_unittest.o 

all:	$(SESSION_BIN) $(SIGNALLER_BIN) $(TEST_BIN)

$(SESSION_BIN): $(DBUS_SERVER) $(SESSION_OBJS)
	$(CXX) $(CXXFLAGS) $(INCLUDE_DIRS) $(LIB_DIRS) $(SESSION_OBJS) \
	 $(LDFLAGS) -o $(SESSION_BIN)

$(SIGNALLER_BIN): $(DBUS_CLIENT) $(SIGNALLER_OBJS)
	$(CXX) $(CXXFLAGS) $(INCLUDE_DIRS) $(LIB_DIRS) $(SIGNALLER_OBJS) \
	 $(LDFLAGS) -o $(SIGNALLER_BIN)

$(TEST_BIN):	$(TEST_OBJS)
	$(CXX) $(CXXFLAGS) $(INCLUDE_DIRS) $(LIB_DIRS) $(TEST_OBJS) \
	 $(LDFLAGS) -lgtest -o $(TEST_BIN)

.cc.o:
	$(CXX) $(CXXFLAGS) $(INCLUDE_DIRS) -c $< -o $@

bindings_dir:
	mkdir -p bindings

$(DBUS_SERVER): $(DBUS_SOURCE) bindings_dir
	dbus-binding-tool --mode=glib-server \
	 --prefix=`basename $(DBUS_SOURCE) .xml` $(DBUS_SOURCE) > $(DBUS_SERVER)

$(DBUS_CLIENT): $(DBUS_SOURCE) bindings_dir
	dbus-binding-tool --mode=glib-client \
	 --prefix=`basename $(DBUS_SOURCE) .xml` $(DBUS_SOURCE) > $(DBUS_CLIENT)

clean:	
	rm -rf *.o $(SESSION_BIN) $(SIGNALLER_BIN) $(TEST_BIN)
