# Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

CXX=g++
LDFLAGS=-lpam -lbase -lpthread -lrt
CXXFLAGS=-Wall -Werror -g
INCLUDE_DIRS=-I../../../third_party/chrome/files
LIB_DIRS=-L../../../third_party/chrome

SESSION_BIN=session
SESSION_OBJS=session.o pam_client.o

EMIT_BIN=emit_login_prompt_ready
EMIT_OBJS=emit_login_prompt_ready.o

all:	$(SESSION_BIN) $(EMIT_BIN)

$(SESSION_BIN):	$(SESSION_OBJS)
	$(CXX) $(CXXFLAGS) $(INCLUDE_DIRS) $(LIB_DIRS) $(SESSION_OBJS) \
	 $(LDFLAGS) -o $(SESSION_BIN)

$(EMIT_BIN):	$(EMIT_OBJS)
	$(CXX) $(CXXFLAGS) $(INCLUDE_DIRS) $(LIB_DIRS) $(EMIT_OBJS) \
	 $(LDFLAGS) -o $(EMIT_BIN)

.cc.o:
	$(CXX) $(CXXFLAGS) $(INCLUDE_DIRS) -c $< -o $@

clean:	
	rm -rf *.o $(SESSION_BIN) $(EMIT_BIN)
