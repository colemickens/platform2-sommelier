// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "chromiumos-wide-profiling/perf_protobuf_io.h"
#include "chromiumos-wide-profiling/perf_reader.h"
#include "chromiumos-wide-profiling/perf_recorder.h"
#include "chromiumos-wide-profiling/perf_serializer.h"
#include "chromiumos-wide-profiling/quipper_string.h"
#include "chromiumos-wide-profiling/quipper_test.h"
#include "chromiumos-wide-profiling/test_utils.h"

namespace quipper {

TEST(PerfRecorderTest, TestRecord) {
  // Read perf data using the PerfReader class.
  // Dump it to a protobuf.
  // Read the protobuf, and reconstruct the perf data.
  quipper::PerfDataProto perf_data_proto;
  string perf_command_line = "sudo " + GetPerfPath() + " record";
  PerfRecorder perf_recorder;
  EXPECT_TRUE(perf_recorder.RecordAndConvertToProtobuf(perf_command_line,
                                                       1,
                                                       &perf_data_proto));
  EXPECT_GT(perf_data_proto.build_ids_size(), 0);
}

}  // namespace quipper

int main(int argc, char* argv[]) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
