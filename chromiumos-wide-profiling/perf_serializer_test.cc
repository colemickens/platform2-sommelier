// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>
#include <sys/time.h>

#include <string>

#include "base/logging.h"

#include "perf_protobuf_io.h"
#include "perf_reader.h"
#include "perf_serializer.h"
#include "perf_test_files.h"
#include "quipper_string.h"
#include "utils.h"

namespace quipper {

namespace {

void SerializeAndDeserialize(const string& input,
                             const string& output,
                             bool do_remap,
                             bool discard_unused_events) {
  quipper::PerfDataProto perf_data_proto;
  PerfSerializer serializer;
  serializer.set_do_remap(do_remap);
  serializer.set_discard_unused_events(discard_unused_events);
  EXPECT_TRUE(serializer.SerializeFromFile(input, &perf_data_proto));

  PerfSerializer deserializer;
  deserializer.set_do_remap(do_remap);
  deserializer.set_discard_unused_events(discard_unused_events);
  EXPECT_TRUE(deserializer.DeserializeToFile(perf_data_proto, output));

  // Check perf event stats.
  const PerfEventStats& in_stats = serializer.stats();
  const PerfEventStats& out_stats = deserializer.stats();
  EXPECT_EQ(in_stats.num_sample_events, out_stats.num_sample_events);
  EXPECT_EQ(in_stats.num_mmap_events, out_stats.num_mmap_events);
  EXPECT_EQ(in_stats.num_fork_events, out_stats.num_fork_events);
  EXPECT_EQ(in_stats.num_exit_events, out_stats.num_exit_events);
  EXPECT_EQ(in_stats.num_sample_events_mapped,
            out_stats.num_sample_events_mapped);
  EXPECT_EQ(do_remap, in_stats.did_remap);
  EXPECT_EQ(do_remap, out_stats.did_remap);
}

void SerializeToFileAndBack(const string& input,
                            const string& output) {
  quipper::PerfDataProto input_perf_data_proto;
  struct timeval pre_serialize_time;
  gettimeofday(&pre_serialize_time, NULL);
  PerfSerializer serializer;
  EXPECT_TRUE(serializer.SerializeFromFile(input, &input_perf_data_proto));

  // Make sure the timestamp_sec was properly recorded.
  EXPECT_TRUE(input_perf_data_proto.has_timestamp_sec());
  // Check it against the current time.
  struct timeval post_serialize_time;
  gettimeofday(&post_serialize_time, NULL);
  EXPECT_GE(input_perf_data_proto.timestamp_sec(), pre_serialize_time.tv_sec);
  EXPECT_LE(input_perf_data_proto.timestamp_sec(), post_serialize_time.tv_sec);

  // Now store the protobuf into a file.
  string input_filename, output_filename;
  EXPECT_TRUE(CreateNamedTempFile(&input_filename));
  EXPECT_TRUE(CreateNamedTempFile(&output_filename));

  EXPECT_TRUE(WriteProtobufToFile(input_perf_data_proto, input_filename));

  quipper::PerfDataProto output_perf_data_proto;
  EXPECT_TRUE(ReadProtobufFromFile(&output_perf_data_proto, input_filename));

  PerfSerializer deserializer;
  EXPECT_TRUE(deserializer.DeserializeToFile(output_perf_data_proto, output));

  EXPECT_TRUE(WriteProtobufToFile(output_perf_data_proto, output_filename));

  EXPECT_NE(GetFileSize(input_filename), 0);
  ASSERT_TRUE(CompareFileContents(input_filename, output_filename));

  remove(input_filename.c_str());
  remove(output_filename.c_str());
}
}  // namespace

TEST(PerfSerializerTest, Test1Cycle) {
  // Read perf data using the PerfReader class.
  // Dump it to a protobuf.
  // Read the protobuf, and reconstruct the perf data.
  // TODO(sque): test exact number of events after discarding unused events.
  for (unsigned int i = 0;
       i < arraysize(perf_test_files::kPerfDataFiles);
       ++i) {
    PerfReader input_perf_reader, output_perf_reader, output_perf_reader1,
               output_perf_reader2;
    quipper::PerfDataProto perf_data_proto, perf_data_proto1;

    string input_perf_data = perf_test_files::kPerfDataFiles[i];
    string output_perf_data = input_perf_data + ".serialized.out";
    string output_perf_data1 = input_perf_data + ".serialized.1.out";

    LOG(INFO) << "Testing " << input_perf_data;
    input_perf_reader.ReadFile(input_perf_data);

    // For every other perf data file, discard unused events.
    bool discard = (i % 2 == 0);

    SerializeAndDeserialize(input_perf_data, output_perf_data, false, discard);
    output_perf_reader.ReadFile(output_perf_data);
    SerializeAndDeserialize(output_perf_data, output_perf_data1, false,
                            discard);
    output_perf_reader1.ReadFile(output_perf_data1);

    ASSERT_TRUE(CompareFileContents(output_perf_data, output_perf_data1));

    string output_perf_data2 = input_perf_data + ".io.out";
    SerializeToFileAndBack(input_perf_data, output_perf_data2);
    output_perf_reader2.ReadFile(output_perf_data2);

    // Make sure the # of events do not increase.  They can decrease because
    // some unused non-sample events may be discarded.
    if (discard) {
      ASSERT_LE(output_perf_reader.events().size(),
                input_perf_reader.events().size());
    } else {
      ASSERT_EQ(output_perf_reader.events().size(),
                input_perf_reader.events().size());
    }
    ASSERT_EQ(output_perf_reader1.events().size(),
              output_perf_reader.events().size());
    ASSERT_EQ(output_perf_reader2.events().size(),
              input_perf_reader.events().size());

    EXPECT_TRUE(ComparePerfReports(input_perf_data, output_perf_data));
    EXPECT_TRUE(ComparePerfBuildIDLists(input_perf_data, output_perf_data));
    EXPECT_TRUE(ComparePerfReports(output_perf_data, output_perf_data2));
    EXPECT_TRUE(ComparePerfBuildIDLists(output_perf_data, output_perf_data2));
  }
}

TEST(PerfSerializerTest, TestRemap) {
  // Read perf data using the PerfReader class with address remapping.
  // Dump it to a protobuf.
  // Read the protobuf, and reconstruct the perf data.
  for (unsigned int i = 0;
       i < arraysize(perf_test_files::kPerfDataFiles);
       ++i) {
    const string input_perf_data = perf_test_files::kPerfDataFiles[i];
    LOG(INFO) << "Testing " << input_perf_data;
    const string output_perf_data = input_perf_data + ".ser.remap.out";
    SerializeAndDeserialize(input_perf_data, output_perf_data, true, true);
  }

  for (unsigned int i = 0;
       i < arraysize(perf_test_files::kPerfPipedDataFiles);
       ++i) {
    const string input_perf_data = perf_test_files::kPerfPipedDataFiles[i];
    LOG(INFO) << "Testing " << input_perf_data;
    const string output_perf_data = input_perf_data + ".ser.remap.out";
    SerializeAndDeserialize(input_perf_data, output_perf_data, true, true);
  }

}

}  // namespace quipper

int main(int argc, char * argv[]) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
