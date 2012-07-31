# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
CXX ?= g++

CXXFLAGS += -g -Wall
LDFLAGS = -ldl -lcurl -lz
TEST_LDFLAGS = $(LDFLAGS) -lgtest

PROGRAM_SUFFIX = exe
TEST_SUFFIX = test

MAIN_SOURCES = quipper.cc
TEST_SOURCES = quipper_unittest.cc

COMMON_SOURCES = profiler.cc uploader.cc parser.cc
COMMON_OBJECTS = $(COMMON_SOURCES:.cc=.o)

PROGRAMS = $(MAIN_SOURCES:.cc=.$(PROGRAM_SUFFIX))
TESTS = $(TEST_SOURCES:.cc=.$(TEST_SUFFIX))

ALL_SOURCES = $(MAIN_SOURCES) $(COMMON_SOURCES) $(TEST_SOURCES)

all: $(PROGRAMS)
	@echo Sources compiled!

-include $(ALL_SOURCES:.cc=.d)

# Taken from:
# http://www.gnu.org/software/make/manual/make.html#Automatic-Prerequisites
%.d: %.cc
	@set -e; rm -f $@; \
	$(CXX) -MM $(CPPFLAGS) $(CXXFLAGS) $< > $@.$$$$; \
	sed 's,\($*\)\.o[ :]*,\1.o $@ : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$

%.exe: %.o $(COMMON_OBJECTS)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(LDFLAGS) -o $@ $^

%.test: %.o $(COMMON_OBJECTS)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(TEST_LDFLAGS) -o $@ $^

check: $(TESTS)
	for test in $(TESTS); do ./$$test || exit 1; done

clean:
	rm -f *.o .d *.a $(PROGRAMS) $(TESTS)
