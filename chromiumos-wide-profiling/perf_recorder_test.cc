// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <gtest/gtest.h>

#include "perf_reader.h"
#include "perf_serializer.h"
#include "perf_protobuf_io.h"
#include "perf_recorder.h"
#include "utils.h"


TEST(PerfRecorderTest, TestRecord) {
  // Read perf data using the PerfReader class.
  // Dump it to a protobuf.
  // Read the protobuf, and reconstruct the perf data.
  PerfDataProto perf_data_proto;
  std::string perf_command_line = "/usr/sbin/perf record";
  PerfRecorder perf_recorder;
  EXPECT_TRUE(perf_recorder.RecordAndConvertToProtobuf(perf_command_line,
                                                       1,
                                                       &perf_data_proto));
}

int main(int argc, char * argv[])
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
