// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>

#include <string>

#include "base/logging.h"

#include "perf_reader.h"
#include "perf_serializer.h"
#include "perf_protobuf_io.h"
#include "utils.h"

namespace {

const char* kPerfDataFiles[] = {
  "perf.data.singleprocess",
  "perf.data.systemwide.1",
  "perf.data.systemwide.5",
  "perf.data.busy.1",
  "perf.data.busy.5",
};

}  // namespace

void SerializeAndDeserialize(const std::string& input,
                             const std::string& output) {
  PerfSerializer perf_serializer;
  PerfDataProto perf_data_proto;
  EXPECT_TRUE(perf_serializer.Serialize(input.c_str(), &perf_data_proto));
  EXPECT_TRUE(perf_serializer.Deserialize(output.c_str(), perf_data_proto));
}

void SerializeToFileAndBack(const std::string& input,
                            const std::string& output) {
  PerfSerializer perf_serializer;
  PerfDataProto input_perf_data_proto, output_perf_data_proto;

  EXPECT_TRUE(perf_serializer.Serialize(input.c_str(), &input_perf_data_proto));
  // Now store the protobuf into a file.
  std::string input_filename, output_filename;
  EXPECT_TRUE(CreateNamedTempFile(&input_filename));
  EXPECT_TRUE(CreateNamedTempFile(&output_filename));

  EXPECT_TRUE(WriteProtobufToFile(input_perf_data_proto, input_filename));

  EXPECT_TRUE(ReadProtobufFromFile(&output_perf_data_proto, input_filename));

  EXPECT_TRUE(perf_serializer.Deserialize(output.c_str(),
                                          output_perf_data_proto));

  EXPECT_TRUE(WriteProtobufToFile(output_perf_data_proto, output_filename));

  EXPECT_NE(GetFileSize(input_filename), 0);
  ASSERT_TRUE(CompareFileContents(input_filename, output_filename));

  remove(input_filename.c_str());
  remove(output_filename.c_str());
}

TEST(PerfReaderTest, Test1Cycle) {
  // Read perf data using the PerfReader class.
  // Dump it to a protobuf.
  // Read the protobuf, and reconstruct the perf data.
  for (unsigned int i = 0; i < arraysize(kPerfDataFiles); ++i) {
    PerfReader input_perf_reader, output_perf_reader, output_perf_reader1,
               output_perf_reader2;
    PerfDataProto perf_data_proto, perf_data_proto1;

    std::string input_perf_data = kPerfDataFiles[i];
    std::string output_perf_data = input_perf_data + ".serialized.out";
    std::string output_perf_data1 = output_perf_data + ".serialized.out";

    LOG(INFO) << "Testing " << input_perf_data;
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
    ASSERT_TRUE(input_perf_reader.events().size() ==
                output_perf_reader.events().size());
    ASSERT_TRUE(input_perf_reader.events().size() ==
                output_perf_reader1.events().size());
    ASSERT_TRUE(input_perf_reader.events().size() ==
                output_perf_reader2.events().size());
    // TODO(sque): check the perf report output also.
  }
}

int main(int argc, char * argv[]) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
