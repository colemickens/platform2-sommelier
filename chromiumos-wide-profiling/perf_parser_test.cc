// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>

#include <string>

#include "base/logging.h"

#include "perf_parser.h"
#include "perf_reader.h"
#include "utils.h"

namespace {

// TODO(sque): this should be defined in one location.
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

TEST(PerfParserTest, Test1Cycle) {
  for (unsigned int i = 0; i < arraysize(kPerfDataFiles); ++i) {
    PerfReader reader;
    string input_perf_data = kPerfDataFiles[i];
    LOG(INFO) << "Testing " << input_perf_data;
    ASSERT_TRUE(reader.ReadFile(input_perf_data));

    PerfParser interpreter;
    interpreter.ParseRawEvents(reader.attrs(), reader.events());

    PerfParser composer;
    composer.GenerateRawEvents(reader.attrs(), interpreter.events());

    PerfReader writer;
    *writer.mutable_attrs() = reader.attrs();
    *writer.mutable_events() = composer.raw_events();
    string output_perf_data = input_perf_data + ".int.out";
    ASSERT_TRUE(writer.WriteFile(output_perf_data));

    EXPECT_TRUE(ComparePerfReports(input_perf_data, output_perf_data));
  }
}

int main(int argc, char* argv[]) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
