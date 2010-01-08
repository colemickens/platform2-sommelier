# Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

CXX=g++
LDFLAGS=-lbase -lpthread -lrt
CXXFLAGS=-Wall -Werror -g
INCLUDE_DIRS=-I../../third_party/chrome/files -I..
LIB_DIRS=-L../../third_party/chrome

SESSION_COMMON_OBJS=session_manager.o child_job.o ipc_channel.o

SESSION_BIN=session_manager
SESSION_OBJS=$(SESSION_COMMON_OBJS) session_manager_main.o 

SIGNALLER_BIN=signaller
SIGNALLER_OBJS=signaller.o ipc_channel.o

TEST_BIN=session_manager_unittest
TEST_OBJS=$(SESSION_COMMON_OBJS) session_manager_testrunner.o \
	child_job_unittest.o session_manager_unittest.o

all:	$(SESSION_BIN) $(SIGNALLER_BIN) $(TEST_BIN)

$(SESSION_BIN):	$(SESSION_OBJS)
	$(CXX) $(CXXFLAGS) $(INCLUDE_DIRS) $(LIB_DIRS) $(SESSION_OBJS) \
	 $(LDFLAGS) -o $(SESSION_BIN)

$(SIGNALLER_BIN): $(SIGNALLER_OBJS)
	$(CXX) $(CXXFLAGS) $(INCLUDE_DIRS) $(LIB_DIRS) $(SIGNALLER_OBJS) \
	 $(LDFLAGS) -o $(SIGNALLER_BIN)

$(TEST_BIN):	$(TEST_OBJS)
	$(CXX) $(CXXFLAGS) $(INCLUDE_DIRS) $(LIB_DIRS) $(TEST_OBJS) \
	 $(LDFLAGS) -lgtest -o $(TEST_BIN)

.cc.o:
	$(CXX) $(CXXFLAGS) $(INCLUDE_DIRS) -c $< -o $@

clean:	
	rm -rf *.o $(SESSION_BIN) $(SIGNALLER_BIN) $(TEST_BIN)
