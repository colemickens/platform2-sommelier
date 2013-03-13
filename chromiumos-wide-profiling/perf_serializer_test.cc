// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>

#include <string>

#include "base/logging.h"

#include "perf_protobuf_io.h"
#include "perf_reader.h"
#include "perf_serializer.h"
#include "perf_test_files.h"
#include "quipper_string.h"
#include "utils.h"

void SerializeAndDeserialize(const string& input,
                             const string& output,
                             bool do_remap) {
  quipper::PerfDataProto perf_data_proto;
  PerfSerializer serializer;
  serializer.set_do_remap(do_remap);
  EXPECT_TRUE(serializer.SerializeFromFile(input, &perf_data_proto));
  PerfSerializer deserializer;
  deserializer.set_do_remap(do_remap);
  EXPECT_TRUE(deserializer.DeserializeToFile(perf_data_proto, output));
}

void SerializeToFileAndBack(const string& input,
                            const string& output) {
  quipper::PerfDataProto input_perf_data_proto;
  PerfSerializer serializer;
  EXPECT_TRUE(serializer.SerializeFromFile(input, &input_perf_data_proto));

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

TEST(PerfSerializerTest, Test1Cycle) {
  // Read perf data using the PerfReader class.
  // Dump it to a protobuf.
  // Read the protobuf, and reconstruct the perf data.
  for (unsigned int i = 0;
       i < arraysize(perf_test_files::kPerfDataFiles);
       ++i) {
    PerfReader input_perf_reader, output_perf_reader, output_perf_reader1,
               output_perf_reader2;
    quipper::PerfDataProto perf_data_proto, perf_data_proto1;

    string input_perf_data = perf_test_files::kPerfDataFiles[i];
    string output_perf_data = input_perf_data + ".serialized.out";
    string output_perf_data1 = output_perf_data + ".serialized.out";

    LOG(INFO) << "Testing " << input_perf_data;
    input_perf_reader.ReadFile(input_perf_data);

    SerializeAndDeserialize(input_perf_data, output_perf_data, false);
    output_perf_reader.ReadFile(output_perf_data);
    SerializeAndDeserialize(output_perf_data, output_perf_data1, false);
    output_perf_reader1.ReadFile(output_perf_data1);

    ASSERT_TRUE(CompareFileContents(output_perf_data, output_perf_data1));

    string output_perf_data2 = input_perf_data + ".io.out";
    SerializeToFileAndBack(input_perf_data, output_perf_data2);
    output_perf_reader2.ReadFile(output_perf_data2);

    // Make sure the # of events do not change.
    ASSERT_EQ(output_perf_reader.events().size(),
              input_perf_reader.events().size());
    ASSERT_EQ(output_perf_reader1.events().size(),
              input_perf_reader.events().size());
    ASSERT_EQ(output_perf_reader2.events().size(),
              input_perf_reader.events().size());

    EXPECT_TRUE(ComparePerfReports(input_perf_data, output_perf_data));
    EXPECT_TRUE(ComparePerfReports(output_perf_data, output_perf_data2));
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
    const string output_perf_data = input_perf_data + ".ser.remap.out";
    SerializeAndDeserialize(input_perf_data, output_perf_data, true);
  }
}

int main(int argc, char * argv[]) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
