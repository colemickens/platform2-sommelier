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
  std::string filename = "/tmp/my.proto.bin";
  std::string filename1 = filename + "1";

  EXPECT_TRUE(WriteProtobufToFile(perf_data_proto,
                                  filename));

  EXPECT_TRUE(ReadProtobufFromFile(&perf_data_proto1,
                                   filename));

  EXPECT_TRUE(perf_serializer.Deserialize(output.c_str(),
                                          &perf_data_proto1));

  EXPECT_TRUE(WriteProtobufToFile(perf_data_proto1,
                                  filename1));

  ASSERT_TRUE(CompareFileContents(filename, filename1));
}

TEST(PerfReaderTest, Test1Cycle) {
  // Read perf data using the PerfReader class.
  // Dump it to a protobuf.
  // Read the protobuf, and reconstruct the perf data.
  PerfDataProto perf_data_proto, perf_data_proto1;

  std::string input_perf_data = "perf.data";
  std::string output_perf_data = input_perf_data + ".serialized.out";
  std::string output_perf_data1 = output_perf_data + ".serialized.out";

  SerializeAndDeserialize(input_perf_data, output_perf_data);
  SerializeAndDeserialize(output_perf_data, output_perf_data1);

  ASSERT_TRUE(CompareFileContents(output_perf_data, output_perf_data1));

  std::string output_perf_data2 = input_perf_data + ".io.out";
  SerializeToFileAndBack(input_perf_data, output_perf_data2);
}

int main(int argc, char * argv[])
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
