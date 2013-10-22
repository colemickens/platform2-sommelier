# Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# Makefile for bootstat utilities
#

# OPT_CFLAGS is here to allow adding options for manual, host-side
# builds.
OPT_CFLAGS =
COMMON_CFLAGS += -D_GNU_SOURCE
COMMON_CFLAGS += -Wall -Werror -fPIC -fno-exceptions $(OPT_CFLAGS)

CFLAGS += $(COMMON_CFLAGS)
CXXFLAGS += $(COMMON_CFLAGS)

LIB_OBJS = bootstat_log.o

COMMAND_OBJS = bootstat.o
COMMAND_LDFLAGS = $(LDFLAGS)

TEST_OBJS = log_unit_tests.o
TEST_LDFLAGS = $(LDFLAGS)
TEST_LDLIBS = -lgtest

OBJS = $(LIB_OBJS) $(TEST_OBJS) $(COMMAND_OBJS)

LIB = libbootstat.a
COMMAND = bootstat
TEST = log_unit_test

%.o: %.c
	$(CC) $(CFLAGS) -o $@ -c $<

%.o: %.cc
	$(CXX) $(CXXFLAGS) -o $@ -c $<

all: $(COMMAND) $(LIB)
tests: $(TEST)

$(COMMAND): $(COMMAND_OBJS) $(LIB)
	$(CC) -o $@ $(COMMAND_LDFLAGS) $^

$(LIB): $(LIB_OBJS)
	$(AR) rcs $@ $^

$(TEST): $(TEST_OBJS) $(LIB)
	$(CXX) $(TEST_LDFLAGS) -o $@ $^ $(TEST_LDLIBS)

clean:
	rm -f $(COMMAND) $(LIB) $(OBJS) $(TEST)
