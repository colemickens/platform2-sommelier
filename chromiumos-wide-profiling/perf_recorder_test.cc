// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "chromiumos-wide-profiling/compat/string.h"
#include "chromiumos-wide-profiling/compat/test.h"
#include "chromiumos-wide-profiling/perf_protobuf_io.h"
#include "chromiumos-wide-profiling/perf_reader.h"
#include "chromiumos-wide-profiling/perf_recorder.h"
#include "chromiumos-wide-profiling/perf_serializer.h"
#include "chromiumos-wide-profiling/run_command.h"
#include "chromiumos-wide-profiling/test_utils.h"

namespace quipper {

// Runs "perf record" to see if the command is available on the current system.
// This should also cover the availability of "perf stat", which is a simpler
// way to get information from the counters.
bool IsPerfRecordAvailable() {
  return RunCommand(
      {"perf", "record", "-a", "-o", "-", "--", "sleep", "1"},
      NULL);
}

TEST(PerfRecorderTest, RecordToProtobuf) {
  // Read perf data using the PerfReader class.
  // Dump it to a string and convert to a protobuf.
  // Read the protobuf, and reconstruct the perf data.
  string output_string;
  PerfRecorder perf_recorder;
  EXPECT_TRUE(perf_recorder.RunCommandAndGetSerializedOutput(
      {"sudo", GetPerfPath(), "record"}, 1, &output_string));

  quipper::PerfDataProto perf_data_proto;
  EXPECT_TRUE(perf_data_proto.ParseFromString(output_string));
  EXPECT_GT(perf_data_proto.build_ids_size(), 0);
}

TEST(PerfRecorderTest, StatToProtobuf) {
  // Run perf stat and verify output.
  string output_string;
  PerfRecorder perf_recorder;
  EXPECT_TRUE(perf_recorder.RunCommandAndGetSerializedOutput(
      {"sudo", GetPerfPath(), "stat"}, 1, &output_string));

  EXPECT_GT(output_string.size(), 0);
  quipper::PerfStatProto stat;
  ASSERT_TRUE(stat.ParseFromString(output_string));
  EXPECT_GT(stat.line_size(), 0);
}

TEST(PerfRecorderTest, StatSingleEvent) {
  string output_string;
  PerfRecorder perf_recorder;
  ASSERT_TRUE(perf_recorder.RunCommandAndGetSerializedOutput(
      {"sudo", GetPerfPath(), "stat", "-a", "-e", "cycles"},
      1, &output_string));

  EXPECT_GT(output_string.size(), 0);

  quipper::PerfStatProto stat;
  ASSERT_TRUE(stat.ParseFromString(output_string));
  // Replace the placeholder "perf" with the actual perf path.
  string expected_command_line =
      string("sudo ") + GetPerfPath() + " stat -a -e cycles -v -- sleep 1";
  EXPECT_EQ(expected_command_line, stat.command_line());

  // Make sure the event counter was read.
  ASSERT_EQ(1, stat.line_size());
  EXPECT_TRUE(stat.line(0).has_time_ms());
  EXPECT_TRUE(stat.line(0).has_count());
  EXPECT_TRUE(stat.line(0).has_event_name());
  // Running for at least one second.
  EXPECT_GE(stat.line(0).time_ms(), 1000);
  EXPECT_EQ("cycles", stat.line(0).event_name());
}

TEST(PerfRecorderTest, StatMultipleEvents) {
  string output_string;
  PerfRecorder perf_recorder;
  ASSERT_TRUE(perf_recorder.RunCommandAndGetSerializedOutput(
      { "sudo", GetPerfPath(), "stat", "-a",
        "-e", "cycles",
        "-e", "instructions",
        "-e", "branches",
        "-e", "branch-misses" },
      2, &output_string));

  EXPECT_GT(output_string.size(), 0);

  quipper::PerfStatProto stat;
  ASSERT_TRUE(stat.ParseFromString(output_string));
  // Replace the placeholder "perf" with the actual perf path.
  string command_line =
      string("sudo ") + GetPerfPath() + " stat -a "
          "-e cycles "
          "-e instructions "
          "-e branches "
          "-e branch-misses "
          "-v "
          "-- sleep 2";
  EXPECT_TRUE(stat.has_command_line());
  EXPECT_EQ(command_line, stat.command_line());

  // Make sure all event counters were read.
  // Check:
  // - Number of events.
  // - Running for at least two seconds.
  // - Event names recorded properly.
  ASSERT_EQ(4, stat.line_size());

  EXPECT_TRUE(stat.line(0).has_time_ms());
  EXPECT_TRUE(stat.line(0).has_count());
  EXPECT_TRUE(stat.line(0).has_event_name());
  EXPECT_GE(stat.line(0).time_ms(), 2000);
  EXPECT_EQ("cycles", stat.line(0).event_name());

  EXPECT_TRUE(stat.line(1).has_time_ms());
  EXPECT_TRUE(stat.line(1).has_count());
  EXPECT_TRUE(stat.line(1).has_event_name());
  EXPECT_GE(stat.line(1).time_ms(), 2000);
  EXPECT_EQ("instructions", stat.line(1).event_name());

  EXPECT_TRUE(stat.line(2).has_time_ms());
  EXPECT_TRUE(stat.line(2).has_count());
  EXPECT_TRUE(stat.line(2).has_event_name());
  EXPECT_GE(stat.line(2).time_ms(), 2000);
  EXPECT_EQ("branches", stat.line(2).event_name());

  EXPECT_TRUE(stat.line(3).has_time_ms());
  EXPECT_TRUE(stat.line(3).has_count());
  EXPECT_TRUE(stat.line(3).has_event_name());
  EXPECT_GE(stat.line(3).time_ms(), 2000);
  EXPECT_EQ("branch-misses", stat.line(3).event_name());
}

TEST(PerfRecorderTest, DontAllowCommands) {
  string output_string;
  PerfRecorder perf_recorder;
  EXPECT_FALSE(perf_recorder.RunCommandAndGetSerializedOutput(
      {"sudo", GetPerfPath(), "record", "--", "sh", "-c", "echo 'malicious'"},
      1, &output_string));
  EXPECT_FALSE(perf_recorder.RunCommandAndGetSerializedOutput(
      {"sudo", GetPerfPath(), "stat", "--", "sh", "-c", "echo 'malicious'"},
      1, &output_string));
}

TEST(PerfRecorderTest, DontAllowOtherPerfSubcommands) {
  string output_string;
  PerfRecorder perf_recorder;
  // Run a few other valid perf commands.
  EXPECT_FALSE(perf_recorder.RunCommandAndGetSerializedOutput(
      {"sudo", GetPerfPath(), "list"},
      1, &output_string));
  EXPECT_FALSE(perf_recorder.RunCommandAndGetSerializedOutput(
      {"sudo", GetPerfPath(), "report"},
      1, &output_string));
  EXPECT_FALSE(perf_recorder.RunCommandAndGetSerializedOutput(
      {"sudo", GetPerfPath(), "trace"},
      1, &output_string));
}

}  // namespace quipper

int main(int argc, char* argv[]) {
  ::testing::InitGoogleTest(&argc, argv);
  if (!quipper::IsPerfRecordAvailable())
    return 0;
  return RUN_ALL_TESTS();
}
