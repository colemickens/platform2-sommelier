// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>
#include <string>

#include "perf_reader.h"

TEST(PerfReaderTest, Test1Cycle) {
  PerfReader pr;
  std::string input_perf_data = "perf.data";
  std::string output_perf_data = input_perf_data + ".pr.out";
  ASSERT_TRUE(pr.ReadFile(input_perf_data));
  ASSERT_TRUE(pr.WriteFile(output_perf_data));
}

int main(int argc, char* argv[]) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
