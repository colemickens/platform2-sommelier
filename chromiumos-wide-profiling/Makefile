# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# TODO(asharif): Convert this to common.mk.
CXX ?= g++

BASE_VERSION ?= 180609

CXXFLAGS += -g -Wall -Iperf -Werror -Wall \
	    $(shell pkg-config --cflags protobuf --cflags libchrome-$(BASE_VERSION))
LDFLAGS += $(shell pkg-config --libs protobuf) \
	   $(shell pkg-config --libs openssl --libs libchrome-$(BASE_VERSION))
TEST_LDFLAGS = $(LDFLAGS) -lgtest

MAIN_SOURCES = quipper.cc
PROGRAMS = $(MAIN_SOURCES:.cc=)
TEST_SOURCES = perf_reader_test.cc perf_serializer_test.cc \
	       perf_recorder_test.cc utils_test.cc
GENERATED_SOURCES = perf_data.pb.cc
GENERATED_HEADERS = $(GENERATED_SOURCES:.pb.cc=.pb.h)

COMMON_SOURCES = utils.cc perf_protobuf_io.cc perf_reader.cc perf_recorder.cc \
		 perf_serializer.cc $(GENERATED_SOURCES)
COMMON_OBJECTS = $(COMMON_SOURCES:.cc=.o)

TESTS = $(TEST_SOURCES:.cc=)

ALL_SOURCES = $(MAIN_SOURCES) $(COMMON_SOURCES) $(TEST_SOURCES)

all: $(PROGRAMS) $(TESTS)
	@echo Sources compiled!

ifneq ($(MAKECMDGOALS),clean)
  -include $(ALL_SOURCES:.cc=.d)
endif

# Taken from:
# http://www.gnu.org/software/make/manual/make.html#Automatic-Prerequisites
%.d: %.cc
	@set -e; rm -f $@; \
	$(CXX) -MM $(CPPFLAGS) $(CXXFLAGS) $< > $@.$$$$; \
	sed 's,\($*\)\.o[ :]*,\1.o $@ : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$

# Rule for compiling protobufs.
%.pb.h %.pb.cc: %.proto
	protoc --cpp_out=. $^

$(PROGRAMS): %: %.o $(COMMON_OBJECTS)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

$(TESTS): %: %.o $(COMMON_OBJECTS)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -o $@ $^ $(TEST_LDFLAGS)

# With large perf.data files, the tests will spew a lot of logging.  The
# following calls will pipe the logging to a log file.  To see test logging,
# build with 'FEATURES="test noclean"' and go to the build directory.
check: $(TESTS)
	for test in $(TESTS); do \
		./$$test 2> ./$$test.stderr.log && continue; \
		cat ./$$test.stderr.log; \
		exit 1; \
	done

clean:
	rm -f *.o *.d.* *.a $(PROGRAMS) $(TESTS) $(GENERATED_SOURCES) \
		$(GENERATED_HEADERS)
