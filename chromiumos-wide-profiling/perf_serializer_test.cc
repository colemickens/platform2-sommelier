// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <gtest/gtest.h>

#include "perf_reader.h"
#include "perf_serializer.h"
#include "perf_protobuf_io.h"
#include "utils.h"

void SerializeAndDeserialize(const std::string & input,
                             const std::string & output) {
  PerfSerializer perf_serializer;
  PerfDataProto perf_data_proto;
  EXPECT_TRUE(perf_serializer.Serialize(input.c_str(),
                                        &perf_data_proto));
  EXPECT_TRUE(perf_serializer.Deserialize(output.c_str(),
                                          &perf_data_proto));
}

void SerializeToFileAndBack(const std::string & input,
                            const std::string & output)
{
  PerfSerializer perf_serializer;
  PerfDataProto perf_data_proto, perf_data_proto1;

  EXPECT_TRUE(perf_serializer.Serialize(input.c_str(),
                                        &perf_data_proto));
  // Now store the protobuf into a file.
  std::string filename, filename1;
  EXPECT_TRUE(CreateNamedTempFile(&filename));
  EXPECT_TRUE(CreateNamedTempFile(&filename1));

  EXPECT_TRUE(WriteProtobufToFile(perf_data_proto,
                                  filename));

  EXPECT_TRUE(ReadProtobufFromFile(&perf_data_proto1,
                                   filename));

  EXPECT_TRUE(perf_serializer.Deserialize(output.c_str(),
                                          &perf_data_proto1));

  EXPECT_TRUE(WriteProtobufToFile(perf_data_proto1,
                                  filename1));

  EXPECT_TRUE(GetFileSize(filename) != 0);
  ASSERT_TRUE(CompareFileContents(filename, filename1));

  remove(filename.c_str());
  remove(filename1.c_str());
}

TEST(PerfReaderTest, Test1Cycle) {
  // Read perf data using the PerfReader class.
  // Dump it to a protobuf.
  // Read the protobuf, and reconstruct the perf data.
  PerfReader input_perf_reader, output_perf_reader, output_perf_reader1,
             output_perf_reader2;
  PerfDataProto perf_data_proto, perf_data_proto1;

  std::string input_perf_data = "perf.data";
  std::string output_perf_data = input_perf_data + ".serialized.out";
  std::string output_perf_data1 = output_perf_data + ".serialized.out";

  input_perf_reader.ReadFile(input_perf_data);

  SerializeAndDeserialize(input_perf_data, output_perf_data);
  output_perf_reader.ReadFile(output_perf_data);
  SerializeAndDeserialize(output_perf_data, output_perf_data1);
  output_perf_reader1.ReadFile(output_perf_data1);

  ASSERT_TRUE(CompareFileContents(output_perf_data, output_perf_data1));

  std::string output_perf_data2 = input_perf_data + ".io.out";
  SerializeToFileAndBack(input_perf_data, output_perf_data2);
  output_perf_reader2.ReadFile(output_perf_data2);

  // Make sure the # of events do not change.
  ASSERT_TRUE(input_perf_reader.get_events().size() ==
              output_perf_reader.get_events().size());
  ASSERT_TRUE(input_perf_reader.get_events().size() ==
              output_perf_reader1.get_events().size());
  ASSERT_TRUE(input_perf_reader.get_events().size() ==
              output_perf_reader2.get_events().size());
}

int main(int argc, char * argv[])
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
