// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>

#include <string>

#include "base/logging.h"

#include "perf_reader.h"
#include "quipper_string.h"
#include "utils.h"

namespace {

const char* kPerfDataFiles[] = {
  "perf.data.singleprocess",
  "perf.data.systemwide.0",
  "perf.data.systemwide.1",
  "perf.data.systemwide.5",
  "perf.data.busy.0",
  "perf.data.busy.1",
  "perf.data.busy.5",
};

}  // namespace

TEST(PerfReaderTest, Test1Cycle) {
  for (unsigned int i = 0; i < arraysize(kPerfDataFiles); ++i) {
    PerfReader pr;
    string input_perf_data = kPerfDataFiles[i];
    LOG(INFO) << "Testing " << input_perf_data;
    string output_perf_data = input_perf_data + ".pr.out";
    ASSERT_TRUE(pr.ReadFile(input_perf_data));
    ASSERT_TRUE(pr.WriteFile(output_perf_data));

    EXPECT_TRUE(ComparePerfReports(input_perf_data, output_perf_data));
  }
}

int main(int argc, char* argv[]) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
