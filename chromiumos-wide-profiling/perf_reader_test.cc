// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <byteswap.h>

#include <algorithm>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "base/logging.h"

#include "chromiumos-wide-profiling/compat/string.h"
#include "chromiumos-wide-profiling/compat/test.h"
#include "chromiumos-wide-profiling/perf_reader.h"
#include "chromiumos-wide-profiling/perf_test_files.h"
#include "chromiumos-wide-profiling/scoped_temp_path.h"
#include "chromiumos-wide-profiling/test_perf_data.h"
#include "chromiumos-wide-profiling/test_utils.h"
#include "chromiumos-wide-profiling/utils.h"

namespace quipper {

TEST(PerfReaderTest, PipedData_IncompleteEventHeader) {
  std::stringstream input;

  // pipe header
  testing::ExamplePipedPerfDataFileHeader().WriteTo(&input);

  // data

  // PERF_RECORD_HEADER_ATTR
  testing::ExamplePerfEventAttrEvent_Hardware(PERF_SAMPLE_IP,
                                              true /*sample_id_all*/)
      .WithConfig(123)
      .WriteTo(&input);

  // PERF_RECORD_HEADER_EVENT_TYPE
  const struct event_type_event event_type = {
    .header = {
      .type = PERF_RECORD_HEADER_EVENT_TYPE,
      .misc = 0,
      .size = sizeof(struct event_type_event),
    },
    .event_type = {
      /*event_id*/ 123,
      /*name*/ "cycles",
    },
  };
  input.write(reinterpret_cast<const char*>(&event_type), sizeof(event_type));

  // Incomplete data at the end:
  input << string(sizeof(struct perf_event_header) - 1, '\0');

  //
  // Parse input.
  //

  PerfReader pr;
  EXPECT_TRUE(pr.ReadFromString(input.str()));

  // Make sure the attr was recorded properly.
  EXPECT_EQ(1, pr.attrs().size());
  EXPECT_EQ(123, pr.attrs()[0].attr.config);
  EXPECT_EQ("cycles", pr.attrs()[0].name);

  // Make sure metadata mask was set to indicate EVENT_TYPE was upgraded
  // to EVENT_DESC.
  EXPECT_EQ((1 << HEADER_EVENT_DESC), pr.metadata_mask());
}

TEST(PerfReaderTest, PipedData_IncompleteEventData) {
  std::stringstream input;

  // pipe header
  testing::ExamplePipedPerfDataFileHeader().WriteTo(&input);

  // data

  // PERF_RECORD_HEADER_ATTR
  testing::ExamplePerfEventAttrEvent_Hardware(PERF_SAMPLE_IP,
                                              true /*sample_id_all*/)
      .WithConfig(456)
      .WriteTo(&input);

  // PERF_RECORD_HEADER_EVENT_TYPE
  const struct event_type_event event_type = {
    .header = {
      .type = PERF_RECORD_HEADER_EVENT_TYPE,
      .misc = 0,
      .size = sizeof(struct event_type_event),
    },
    .event_type = {
      /*event_id*/ 456,
      /*name*/ "instructions",
    },
  };
  input.write(reinterpret_cast<const char*>(&event_type), sizeof(event_type));

  // Incomplete data at the end:
  // Header:
  const struct perf_event_header incomplete_event_header = {
    .type = PERF_RECORD_SAMPLE,
    .misc = 0,
    .size = sizeof(perf_event_header) + 10,
  };
  input.write(reinterpret_cast<const char*>(&incomplete_event_header),
              sizeof(incomplete_event_header));
  // Incomplete event:
  input << string(3, '\0');

  //
  // Parse input.
  //

  PerfReader pr;
  EXPECT_TRUE(pr.ReadFromString(input.str()));

  // Make sure the attr was recorded properly.
  EXPECT_EQ(1, pr.attrs().size());
  EXPECT_EQ(456, pr.attrs()[0].attr.config);
  EXPECT_EQ("instructions", pr.attrs()[0].name);

  // Make sure metadata mask was set to indicate EVENT_TYPE was upgraded
  // to EVENT_DESC.
  EXPECT_EQ((1 << HEADER_EVENT_DESC), pr.metadata_mask());
}

TEST(PerfReaderTest, CorruptedFiles) {
  for (const char* test_file : perf_test_files::kCorruptedPerfPipedDataFiles) {
    string input_perf_data = GetTestInputFilePath(test_file);
    LOG(INFO) << "Testing " << input_perf_data;
    ASSERT_TRUE(FileExists(input_perf_data)) << "Test file does not exist!";
    PerfReader pr;
    ASSERT_FALSE(pr.ReadFile(input_perf_data));
  }
}

TEST(PerfReaderTest, PerfizeBuildID) {
  string build_id_string = "f";
  PerfReader::PerfizeBuildIDString(&build_id_string);
  EXPECT_EQ("f000000000000000000000000000000000000000", build_id_string);
  PerfReader::PerfizeBuildIDString(&build_id_string);
  EXPECT_EQ("f000000000000000000000000000000000000000", build_id_string);

  build_id_string = "01234567890123456789012345678901234567890";
  PerfReader::PerfizeBuildIDString(&build_id_string);
  EXPECT_EQ("0123456789012345678901234567890123456789", build_id_string);
  PerfReader::PerfizeBuildIDString(&build_id_string);
  EXPECT_EQ("0123456789012345678901234567890123456789", build_id_string);
}

TEST(PerfReaderTest, UnperfizeBuildID) {
  string build_id_string = "f000000000000000000000000000000000000000";
  PerfReader::TrimZeroesFromBuildIDString(&build_id_string);
  EXPECT_EQ("f0000000", build_id_string);
  PerfReader::TrimZeroesFromBuildIDString(&build_id_string);
  EXPECT_EQ("f0000000", build_id_string);

  build_id_string = "0123456789012345678901234567890123456789";
  PerfReader::TrimZeroesFromBuildIDString(&build_id_string);
  EXPECT_EQ("0123456789012345678901234567890123456789", build_id_string);

  build_id_string = "0000000000000000000000000000001000000000";
  PerfReader::TrimZeroesFromBuildIDString(&build_id_string);
  EXPECT_EQ("00000000000000000000000000000010", build_id_string);
  PerfReader::TrimZeroesFromBuildIDString(&build_id_string);
  EXPECT_EQ("00000000000000000000000000000010", build_id_string);

  build_id_string = "0000000000000000000000000000000000000000";  // 40 zeroes
  PerfReader::TrimZeroesFromBuildIDString(&build_id_string);
  EXPECT_EQ("", build_id_string);

  build_id_string = "00000000000000000000000000000000";  // 32 zeroes
  PerfReader::TrimZeroesFromBuildIDString(&build_id_string);
  EXPECT_EQ("", build_id_string);

  build_id_string = "00000000";  // 8 zeroes
  PerfReader::TrimZeroesFromBuildIDString(&build_id_string);
  EXPECT_EQ("", build_id_string);

  build_id_string = "0000000";  // 7 zeroes
  PerfReader::TrimZeroesFromBuildIDString(&build_id_string);
  EXPECT_EQ("0000000", build_id_string);

  build_id_string = "";
  PerfReader::TrimZeroesFromBuildIDString(&build_id_string);
  EXPECT_EQ("", build_id_string);
}

TEST(PerfReaderTest, ReadsAndWritesTraceMetadata) {
  std::stringstream input;

  const size_t data_size =
      testing::ExamplePerfSampleEvent_Tracepoint::kEventSize;

  // header
  testing::ExamplePerfDataFileHeader file_header(1 << HEADER_TRACING_DATA);
  file_header
      .WithAttrCount(1)
      .WithDataSize(data_size);
  file_header.WriteTo(&input);
  const perf_file_header &header = file_header.header();

  // attrs
  CHECK_EQ(header.attrs.offset, static_cast<u64>(input.tellp()));
  testing::ExamplePerfFileAttr_Tracepoint(73).WriteTo(&input);

  // data
  ASSERT_EQ(header.data.offset, static_cast<u64>(input.tellp()));
  testing::ExamplePerfSampleEvent_Tracepoint().WriteTo(&input);
  ASSERT_EQ(file_header.data_end(), input.tellp());

  // metadata

  const unsigned int metadata_count = 1;

  // HEADER_TRACING_DATA
  testing::ExampleTracingMetadata tracing_metadata(
      file_header.data_end() + metadata_count*sizeof(perf_file_section));

  // write metadata index entries
  tracing_metadata.index_entry().WriteTo(&input);
  // write metadata
  tracing_metadata.data().WriteTo(&input);

  //
  // Parse input.
  //

  PerfReader pr;
  EXPECT_TRUE(pr.ReadFromString(input.str()));
  EXPECT_EQ(tracing_metadata.data().value(), pr.tracing_data());

  // Write it out and read it in again, it should still be good:
  std::vector<char> output_perf_data;
  EXPECT_TRUE(pr.WriteToVector(&output_perf_data));
  EXPECT_TRUE(pr.ReadFromVector(output_perf_data));
  EXPECT_EQ(tracing_metadata.data().value(), pr.tracing_data());
}

TEST(PerfReaderTest, ReadsTracingMetadataEvent) {
  std::stringstream input;

  // pipe header
  testing::ExamplePipedPerfDataFileHeader().WriteTo(&input);

  // data

  const char x[] = "\x17\x08\x44tracing0.5BLAHBLAHBLAH....";
  const std::vector<char> trace_metadata(x, x+sizeof(x)-1);

  const tracing_data_event trace_event = {
    .header = {
      .type = PERF_RECORD_HEADER_TRACING_DATA,
      .misc = 0,
      .size = sizeof(tracing_data_event),
    },
    .size = static_cast<u32>(trace_metadata.size()),
  };

  input.write(reinterpret_cast<const char*>(&trace_event), sizeof(trace_event));
  input.write(trace_metadata.data(), trace_metadata.size());

  //
  // Parse input.
  //

  PerfReader pr;
  EXPECT_TRUE(pr.ReadFromString(input.str()));
  EXPECT_EQ(trace_metadata, pr.tracing_data());

  // Write it out and read it in again, tracing_data() should still be correct.
  // NB: It does not get written as an event, but in a metadata section.
  std::vector<char> output_perf_data;
  EXPECT_TRUE(pr.WriteToVector(&output_perf_data));
  EXPECT_TRUE(pr.ReadFromVector(output_perf_data));
  EXPECT_EQ(trace_metadata, pr.tracing_data());
}

// Regression test for http://crbug.com/484393
TEST(PerfReaderTest, BranchStackMetadataIndexHasZeroSize) {
  std::stringstream input;

  const size_t data_size =
      testing::ExamplePerfSampleEvent_BranchStack::kEventSize;

  // header
  testing::ExamplePerfDataFileHeader file_header(1 << HEADER_BRANCH_STACK);
  file_header
      .WithAttrCount(1)
      .WithDataSize(data_size);
  file_header.WriteTo(&input);
  const perf_file_header &header = file_header.header();

  // attrs
  CHECK_EQ(header.attrs.offset, static_cast<u64>(input.tellp()));
  testing::ExamplePerfFileAttr_Hardware(
      PERF_SAMPLE_BRANCH_STACK, false /*sample_id_all*/).WriteTo(&input);

  // data
  ASSERT_EQ(header.data.offset, static_cast<u64>(input.tellp()));
  testing::ExamplePerfSampleEvent_BranchStack().WriteTo(&input);
  ASSERT_EQ(file_header.data_end(), input.tellp());

  // metadata

  // HEADER_BRANCH_STACK
  const perf_file_section branch_stack_index = {
    .offset = file_header.data_end_offset(),
    .size = 0,
  };
  input.write(reinterpret_cast<const char*>(&branch_stack_index),
              sizeof(branch_stack_index));

  //
  // Parse input.
  //

  PerfReader pr;
  EXPECT_TRUE(pr.ReadFromString(input.str()));

  // Write it out again.
  // Initialize the buffer to a non-zero sentinel value so that the bytes
  // we are checking were written with zero must have been written.
  const size_t max_predicted_written_size = 1024;
  std::vector<char> output_perf_data(max_predicted_written_size, '\xaa');
  EXPECT_TRUE(pr.WriteToVector(&output_perf_data));
  EXPECT_LE(output_perf_data.size(), max_predicted_written_size)
      << "Bad prediction for written size";

  // Specifically check that the metadata index has zero in the size.
  const auto *output_header =
      reinterpret_cast<struct perf_file_header*>(output_perf_data.data());
  // HEADER_EVENT_DESC gets added.
  const u64 output_features = 1 << HEADER_EVENT_DESC | 1 << HEADER_BRANCH_STACK;
  EXPECT_EQ(output_features, output_header->adds_features[0])
      << "Expected just a HEADER_BRANCH_STACK feature";
  const size_t metadata_offset =
      output_header->data.offset + output_header->data.size;
  const auto *output_feature_index =
      reinterpret_cast<struct perf_file_section*>(
          output_perf_data.data() + metadata_offset);
  EXPECT_EQ(0, output_feature_index[1].size)
      << "Regression: Expected zero size for the HEADER_BRANCH_STACK feature "
      << "metadata index";
}

// Regression test for http://crbug.com/427767
TEST(PerfReaderTest, CorrectlyReadsPerfEventAttrSize) {
  std::stringstream input;

  // pipe header
  testing::ExamplePipedPerfDataFileHeader().WriteTo(&input);

  // data

  struct old_perf_event_attr {
    __u32 type;
    __u32 size;
    __u64 config;
    // union {
            __u64 sample_period;
    //      __u64 sample_freq;
    // };
    __u64 sample_type;
    __u64 read_format;
    // Skip the rest of the fields from perf_event_attr to simulate an
    // older, smaller version of the struct.
  };

  struct old_attr_event {
    struct perf_event_header header;
    struct old_perf_event_attr attr;
    u64 id[];
  };


  const old_attr_event attr = {
    .header = {
      .type = PERF_RECORD_HEADER_ATTR,
      .misc = 0,
      // A count of 8 ids is carefully selected to make the event exceed
      // 96 bytes (sizeof(perf_event_attr)) so that the test fails instead of
      // crashes with the old code.
      .size = sizeof(old_attr_event) + 8*sizeof(u64),
    },
    .attr = {
      .type = 0,
      .size = sizeof(old_perf_event_attr),
      .config = 0,
      .sample_period = 10000001,
      .sample_type = PERF_SAMPLE_IP | PERF_SAMPLE_TID | PERF_SAMPLE_TIME |
                     PERF_SAMPLE_ID | PERF_SAMPLE_CPU,
      .read_format = PERF_FORMAT_ID,
    },
  };

  input.write(reinterpret_cast<const char*>(&attr), sizeof(attr));
  for (u64 id : {301, 302, 303, 304, 305, 306, 307, 308})
    input.write(reinterpret_cast<const char*>(&id), sizeof(id));

  // Add some sample events so that there's something to over-read.
  const sample_event sample = {
    .header = {
      .type = PERF_RECORD_SAMPLE,
      .misc = 0,
      .size = sizeof(perf_event_header) + 5*sizeof(u64),
    }
  };

  for (int i = 0; i < 20; i++) {
    input.write(reinterpret_cast<const char*>(&sample), sizeof(sample));
    u64 qword = 0;
    for (int j = 0; j < 5; j++)
      input.write(reinterpret_cast<const char*>(&qword), sizeof(qword));
  }

  //
  // Parse input.
  //

  PerfReader pr;
  EXPECT_TRUE(pr.ReadFromString(input.str()));
  ASSERT_EQ(pr.attrs().size(), 1);
  const PerfFileAttr& actual_attr = pr.attrs()[0];
  ASSERT_EQ(8, actual_attr.ids.size());
  EXPECT_EQ(301, actual_attr.ids[0]);
  EXPECT_EQ(302, actual_attr.ids[1]);
  EXPECT_EQ(303, actual_attr.ids[2]);
  EXPECT_EQ(304, actual_attr.ids[3]);
}

TEST(PerfReaderTest, ReadsAndWritesSampleAndSampleIdAll) {
  using testing::PunU32U64;

  std::stringstream input;

  // header
  testing::ExamplePipedPerfDataFileHeader().WriteTo(&input);

  // data

  // PERF_RECORD_HEADER_ATTR
  const u64 sample_type =      // * == in sample_id_all
      PERF_SAMPLE_IP |
      PERF_SAMPLE_TID |        // *
      PERF_SAMPLE_TIME |       // *
      PERF_SAMPLE_ADDR |
      PERF_SAMPLE_ID |         // *
      PERF_SAMPLE_STREAM_ID |  // *
      PERF_SAMPLE_CPU |        // *
      PERF_SAMPLE_PERIOD;
  const size_t num_sample_event_bits = 8;
  const size_t num_sample_id_bits = 5;
  // not tested:
  // PERF_SAMPLE_READ |
  // PERF_SAMPLE_RAW |
  // PERF_SAMPLE_CALLCHAIN |
  // PERF_SAMPLE_BRANCH_STACK |
  testing::ExamplePerfEventAttrEvent_Hardware(sample_type,
                                              true /*sample_id_all*/)
      .WriteTo(&input);

  // PERF_RECORD_SAMPLE
  const sample_event written_sample_event = {
    .header = {
      .type = PERF_RECORD_SAMPLE,
      .misc = PERF_RECORD_MISC_KERNEL,
      .size = sizeof(struct sample_event) + num_sample_event_bits*sizeof(u64),
    }
  };
  const u64 sample_event_array[] = {
    0xffffffff01234567,                    // IP
    PunU32U64{.v32 = {0x68d, 0x68e}}.v64,  // TID (u32 pid, tid)
    1415837014*1000000000ULL,              // TIME
    0x00007f999c38d15a,                    // ADDR
    2,                                     // ID
    1,                                     // STREAM_ID
    8,                                     // CPU
    10001,                                 // PERIOD
  };
  ASSERT_EQ(written_sample_event.header.size,
            sizeof(written_sample_event.header) + sizeof(sample_event_array));
  input.write(reinterpret_cast<const char*>(&written_sample_event),
              sizeof(written_sample_event));
  input.write(reinterpret_cast<const char*>(sample_event_array),
              sizeof(sample_event_array));

  // PERF_RECORD_MMAP
  ASSERT_EQ(40, offsetof(struct mmap_event, filename));
  const size_t mmap_event_size =
      offsetof(struct mmap_event, filename) +
      10+6 /* ==16, nearest 64-bit boundary for filename */ +
      num_sample_id_bits*sizeof(u64);

  struct mmap_event written_mmap_event = {
    .header = {
      .type = PERF_RECORD_MMAP,
      .misc = 0,
      .size = mmap_event_size,
    },
    .pid = 0x68d, .tid = 0x68d,
    .start = 0x1d000,
    .len = 0x1000,
    .pgoff = 0,
    // .filename = ..., // written separately
  };
  const char mmap_filename[10+6] = "/dev/zero";
  const u64 mmap_sample_id[] = {
    PunU32U64{.v32 = {0x68d, 0x68e}}.v64,  // TID (u32 pid, tid)
    1415911367*1000000000ULL,              // TIME
    3,                                     // ID
    2,                                     // STREAM_ID
    9,                                     // CPU
  };
  const size_t pre_mmap_offset = input.tellp();
  input.write(reinterpret_cast<const char*>(&written_mmap_event),
              offsetof(struct mmap_event, filename));
  input.write(mmap_filename, 10+6);
  input.write(reinterpret_cast<const char*>(mmap_sample_id),
              sizeof(mmap_sample_id));
  const size_t written_mmap_size =
      static_cast<size_t>(input.tellp()) - pre_mmap_offset;
  ASSERT_EQ(written_mmap_event.header.size,
            static_cast<u64>(written_mmap_size));

  //
  // Parse input.
  //

  struct perf_sample sample;

  PerfReader pr1;
  EXPECT_TRUE(pr1.ReadFromString(input.str()));
  // Write it out and read it in again, the two should have the same data.
  std::vector<char> output_perf_data;
  EXPECT_TRUE(pr1.WriteToVector(&output_perf_data));
  PerfReader pr2;
  EXPECT_TRUE(pr2.ReadFromVector(output_perf_data));

  // Test both versions:
  for (PerfReader* pr : {&pr1, &pr2}) {
    // PERF_RECORD_HEADER_ATTR is added to attr(), not events().
    EXPECT_EQ(2, pr->events().size());

    const event_t* sample_event = pr->events()[0].get();
    EXPECT_EQ(PERF_RECORD_SAMPLE, sample_event->header.type);
    EXPECT_TRUE(pr->ReadPerfSampleInfo(*sample_event, &sample));
    EXPECT_EQ(0xffffffff01234567, sample.ip);
    EXPECT_EQ(0x68d, sample.pid);
    EXPECT_EQ(0x68e, sample.tid);
    EXPECT_EQ(1415837014*1000000000ULL, sample.time);
    EXPECT_EQ(0x00007f999c38d15a, sample.addr);
    EXPECT_EQ(2, sample.id);
    EXPECT_EQ(1, sample.stream_id);
    EXPECT_EQ(8, sample.cpu);
    EXPECT_EQ(10001, sample.period);

    const event_t* mmap_event = pr->events()[1].get();
    EXPECT_EQ(PERF_RECORD_MMAP, mmap_event->header.type);
    EXPECT_TRUE(pr->ReadPerfSampleInfo(*mmap_event, &sample));
    EXPECT_EQ(0x68d, sample.pid);
    EXPECT_EQ(0x68e, sample.tid);
    EXPECT_EQ(1415911367*1000000000ULL, sample.time);
    EXPECT_EQ(3, sample.id);
    EXPECT_EQ(2, sample.stream_id);
    EXPECT_EQ(9, sample.cpu);
  }
}

// Test that PERF_SAMPLE_IDENTIFIER is parsed correctly. This field
// is in a different place in PERF_RECORD_SAMPLE events compared to the
// struct sample_id placed at the end of all other events.
TEST(PerfReaderTest, ReadsAndWritesPerfSampleIdentifier) {
  std::stringstream input;

  // header
  testing::ExamplePipedPerfDataFileHeader().WriteTo(&input);

  // data

  // PERF_RECORD_HEADER_ATTR
  testing::ExamplePerfEventAttrEvent_Hardware(PERF_SAMPLE_IDENTIFIER |
                                              PERF_SAMPLE_IP |
                                              PERF_SAMPLE_TID,
                                              true /*sample_id_all*/)
      .WriteTo(&input);

  // PERF_RECORD_SAMPLE
  const sample_event written_sample_event = {
    .header = {
      .type = PERF_RECORD_SAMPLE,
      .misc = PERF_RECORD_MISC_KERNEL,
      .size = sizeof(struct sample_event) + 3*sizeof(u64),
    }
  };
  const u64 sample_event_array[] = {
    0x00000000deadbeef,  // IDENTIFIER
    0x00007f999c38d15a,  // IP
    0x0000068d0000068d,  // TID (u32 pid, tid)
  };
  ASSERT_EQ(written_sample_event.header.size,
            sizeof(written_sample_event.header) + sizeof(sample_event_array));
  input.write(reinterpret_cast<const char*>(&written_sample_event),
              sizeof(written_sample_event));
  input.write(reinterpret_cast<const char*>(sample_event_array),
              sizeof(sample_event_array));

  // PERF_RECORD_MMAP
  ASSERT_EQ(40, offsetof(struct mmap_event, filename));
  const size_t mmap_event_size =
      offsetof(struct mmap_event, filename) +
      10+6 /* ==16, nearest 64-bit boundary for filename */ +
      2*sizeof(u64);

  struct mmap_event written_mmap_event = {
    .header = {
      .type = PERF_RECORD_MMAP,
      .misc = 0,
      .size = mmap_event_size,
    },
    .pid = 0x68d, .tid = 0x68d,
    .start = 0x1d000,
    .len = 0x1000,
    .pgoff = 0,
    // .filename = ..., // written separately
  };
  const char mmap_filename[10+6] = "/dev/zero";
  const u64 mmap_sample_id[] = {
    // NB: PERF_SAMPLE_IP is not part of sample_id
    0x0000068d0000068d,  // TID (u32 pid, tid)
    0x00000000f00dbaad,  // IDENTIFIER
  };
  const size_t pre_mmap_offset = input.tellp();
  input.write(reinterpret_cast<const char*>(&written_mmap_event),
              offsetof(struct mmap_event, filename));
  input.write(mmap_filename, 10+6);
  input.write(reinterpret_cast<const char*>(mmap_sample_id),
              sizeof(mmap_sample_id));
  const size_t written_mmap_size =
      static_cast<size_t>(input.tellp()) - pre_mmap_offset;
  ASSERT_EQ(written_mmap_event.header.size,
            static_cast<u64>(written_mmap_size));

  //
  // Parse input.
  //

  struct perf_sample sample;

  PerfReader pr1;
  EXPECT_TRUE(pr1.ReadFromString(input.str()));
  // Write it out and read it in again, the two should have the same data.
  std::vector<char> output_perf_data;
  EXPECT_TRUE(pr1.WriteToVector(&output_perf_data));
  PerfReader pr2;
  EXPECT_TRUE(pr2.ReadFromVector(output_perf_data));

  // Test both versions:
  for (PerfReader* pr : {&pr1, &pr2}) {
    // PERF_RECORD_HEADER_ATTR is added to attr(), not events().
    EXPECT_EQ(2, pr->events().size());

    const event_t* sample_event = pr->events()[0].get();
    EXPECT_EQ(PERF_RECORD_SAMPLE, sample_event->header.type);
    EXPECT_TRUE(pr->ReadPerfSampleInfo(*sample_event, &sample));
    EXPECT_EQ(0xdeadbeefULL, sample.id);

    const event_t* mmap_event = pr->events()[1].get();
    EXPECT_EQ(PERF_RECORD_MMAP, mmap_event->header.type);
    EXPECT_TRUE(pr->ReadPerfSampleInfo(*mmap_event, &sample));
    EXPECT_EQ(0xf00dbaadULL, sample.id);
  }
}

TEST(PerfReaderTest, ReadsAndWritesMmap2Events) {
  std::stringstream input;

  // header
  testing::ExamplePipedPerfDataFileHeader().WriteTo(&input);

  // data

  // PERF_RECORD_HEADER_ATTR
  testing::ExamplePerfEventAttrEvent_Hardware(PERF_SAMPLE_IP,
                                              false /*sample_id_all*/)
      .WriteTo(&input);

  // PERF_RECORD_MMAP2
  ASSERT_EQ(72, offsetof(struct mmap2_event, filename));
  const size_t mmap_event_size =
      offsetof(struct mmap2_event, filename) +
      10+6; /* ==16, nearest 64-bit boundary for filename */

  struct mmap2_event written_mmap_event = {
    .header = {
      .type = PERF_RECORD_MMAP2,
      .misc = 0,
      .size = mmap_event_size,
    },
    .pid = 0x68d, .tid = 0x68d,
    .start = 0x1d000,
    .len = 0x1000,
    .pgoff = 0,
    .maj = 6,
    .min = 7,
    .ino = 8,
    .ino_generation = 9,
    .prot = 1|2,  // == PROT_READ | PROT_WRITE
    .flags = 2,   // == MAP_PRIVATE
    // .filename = ..., // written separately
  };
  const char mmap_filename[10+6] = "/dev/zero";
  const size_t pre_mmap_offset = input.tellp();
  input.write(reinterpret_cast<const char*>(&written_mmap_event),
              offsetof(struct mmap2_event, filename));
  input.write(mmap_filename, 10+6);
  const size_t written_mmap_size =
      static_cast<size_t>(input.tellp()) - pre_mmap_offset;
  ASSERT_EQ(written_mmap_event.header.size,
            static_cast<u64>(written_mmap_size));

  //
  // Parse input.
  //

  PerfReader pr1;
  EXPECT_TRUE(pr1.ReadFromString(input.str()));
  // Write it out and read it in again, the two should have the same data.
  std::vector<char> output_perf_data;
  EXPECT_TRUE(pr1.WriteToVector(&output_perf_data));
  PerfReader pr2;
  EXPECT_TRUE(pr2.ReadFromVector(output_perf_data));

  // Test both versions:
  for (PerfReader* pr : {&pr1, &pr2}) {
    // PERF_RECORD_HEADER_ATTR is added to attr(), not events().
    EXPECT_EQ(1, pr->events().size());

    const event_t* mmap2_event = pr->events()[0].get();
    EXPECT_EQ(PERF_RECORD_MMAP2, mmap2_event->header.type);
    EXPECT_EQ(0x68d, mmap2_event->mmap2.pid);
    EXPECT_EQ(0x68d, mmap2_event->mmap2.tid);
    EXPECT_EQ(0x1d000, mmap2_event->mmap2.start);
    EXPECT_EQ(0x1000, mmap2_event->mmap2.len);
    EXPECT_EQ(0, mmap2_event->mmap2.pgoff);
    EXPECT_EQ(6, mmap2_event->mmap2.maj);
    EXPECT_EQ(7, mmap2_event->mmap2.min);
    EXPECT_EQ(8, mmap2_event->mmap2.ino);
    EXPECT_EQ(9, mmap2_event->mmap2.ino_generation);
    EXPECT_EQ(1|2, mmap2_event->mmap2.prot);
    EXPECT_EQ(2, mmap2_event->mmap2.flags);
  }
}

// Regression test for http://crbug.com/493533
TEST(PerfReaderTest, ReadsAllAvailableMetadataTypes) {
  std::stringstream input;

  const uint32_t features = (1 << HEADER_HOSTNAME) |
                            (1 << HEADER_OSRELEASE) |
                            (1 << HEADER_VERSION) |
                            (1 << HEADER_ARCH) |
                            (1 << HEADER_LAST_FEATURE);

  // header
  testing::ExamplePerfDataFileHeader file_header(features);
  file_header.WriteTo(&input);

  // metadata

  size_t metadata_offset =
      file_header.data_end() + 5 * sizeof(perf_file_section);

  // HEADER_HOSTNAME
  testing::ExampleStringMetadata hostname_metadata("hostname", metadata_offset);
  metadata_offset += hostname_metadata.size();

  // HEADER_OSRELEASE
  testing::ExampleStringMetadata osrelease_metadata("osrelease",
                                                    metadata_offset);
  metadata_offset += osrelease_metadata.size();

  // HEADER_VERSION
  testing::ExampleStringMetadata version_metadata("version", metadata_offset);
  metadata_offset += version_metadata.size();

  // HEADER_ARCH
  testing::ExampleStringMetadata arch_metadata("arch", metadata_offset);
  metadata_offset += arch_metadata.size();

  // HEADER_LAST_FEATURE -- this is just a dummy metadata that will be skipped
  // over. In practice, there will not actually be a metadata entry of type
  // HEADER_LAST_FEATURE. But use because it will never become a supported
  // metadata type.
  testing::ExampleStringMetadata last_feature("*unsupported*", metadata_offset);
  metadata_offset += last_feature.size();

  hostname_metadata.index_entry().WriteTo(&input);
  osrelease_metadata.index_entry().WriteTo(&input);
  version_metadata.index_entry().WriteTo(&input);
  arch_metadata.index_entry().WriteTo(&input);
  last_feature.index_entry().WriteTo(&input);

  hostname_metadata.WriteTo(&input);
  osrelease_metadata.WriteTo(&input);
  version_metadata.WriteTo(&input);
  arch_metadata.WriteTo(&input);
  last_feature.WriteTo(&input);

  //
  // Parse input.
  //

  PerfReader pr;
  EXPECT_TRUE(pr.ReadFromString(input.str()));

  const auto& string_metadata = pr.string_metadata();

  // The dummy metadata should not have been stored.
  EXPECT_EQ(4, string_metadata.size());

  // Make sure each metadata entry has a stored string value.
  for (const auto& entry : string_metadata)
    EXPECT_EQ(1, entry.data.size());

  EXPECT_EQ(HEADER_HOSTNAME, string_metadata[0].type);
  EXPECT_EQ("hostname", string_metadata[0].data[0].str);

  EXPECT_EQ(HEADER_OSRELEASE, string_metadata[1].type);
  EXPECT_EQ("osrelease", string_metadata[1].data[0].str);

  EXPECT_EQ(HEADER_VERSION, string_metadata[2].type);
  EXPECT_EQ("version", string_metadata[2].data[0].str);

  EXPECT_EQ(HEADER_ARCH, string_metadata[3].type);
  EXPECT_EQ("arch", string_metadata[3].data[0].str);
}

// Regression test for http://crbug.com/493533
TEST(PerfReaderTest, ReadsAllAvailableMetadataTypesPiped) {
  std::stringstream input;

  // pipe header
  testing::ExamplePipedPerfDataFileHeader().WriteTo(&input);

  // metadata

  // Provide these out of order. Order should not matter to the PerfReader, but
  // this tests whether the reader is capable of skipping an unsupported type.
  testing::ExampleStringMetadataEvent(PERF_RECORD_HEADER_HOSTNAME, "hostname")
      .WriteTo(&input);
  testing::ExampleStringMetadataEvent(PERF_RECORD_HEADER_OSRELEASE, "osrelease")
      .WriteTo(&input);
  testing::ExampleStringMetadataEvent(PERF_RECORD_HEADER_MAX, "*unsupported*")
      .WriteTo(&input);
  testing::ExampleStringMetadataEvent(PERF_RECORD_HEADER_VERSION, "version")
      .WriteTo(&input);
  testing::ExampleStringMetadataEvent(PERF_RECORD_HEADER_ARCH, "arch")
      .WriteTo(&input);

  //
  // Parse input.
  //

  PerfReader pr;
  EXPECT_TRUE(pr.ReadFromString(input.str()));

  const auto& string_metadata = pr.string_metadata();

  // The dummy metadata should not have been stored.
  EXPECT_EQ(4, string_metadata.size());

  // Make sure each metadata entry has a stored string value.
  for (const auto& entry : string_metadata)
    EXPECT_EQ(1, entry.data.size());

  EXPECT_EQ(HEADER_HOSTNAME, string_metadata[0].type);
  EXPECT_EQ("hostname", string_metadata[0].data[0].str);

  EXPECT_EQ(HEADER_OSRELEASE, string_metadata[1].type);
  EXPECT_EQ("osrelease", string_metadata[1].data[0].str);

  EXPECT_EQ(HEADER_VERSION, string_metadata[2].type);
  EXPECT_EQ("version", string_metadata[2].data[0].str);

  EXPECT_EQ(HEADER_ARCH, string_metadata[3].type);
  EXPECT_EQ("arch", string_metadata[3].data[0].str);
}

// Regression test for http://crbug.com/496441
TEST(PerfReaderTest, LargePerfEventAttr) {
  std::stringstream input;

  const size_t attr_size = sizeof(perf_event_attr) + sizeof(u64);
  testing::ExamplePerfSampleEvent sample_event(
      testing::SampleInfo().Ip(0x00000000002c100a).Tid(1002));
  const size_t data_size = sample_event.GetSize();

  // header
  testing::ExamplePerfDataFileHeader file_header(0);
  file_header
      .WithCustomPerfEventAttrSize(attr_size)
      .WithAttrCount(1)
      .WithDataSize(data_size);
  file_header.WriteTo(&input);

  // attrs
  CHECK_EQ(file_header.header().attrs.offset,
           static_cast<u64>(input.tellp()));
  testing::ExamplePerfFileAttr_Hardware(PERF_SAMPLE_IP | PERF_SAMPLE_TID,
                                        false /*sample_id_all*/)
      .WithAttrSize(attr_size)
      .WithConfig(456)
      .WriteTo(&input);

  // data

  ASSERT_EQ(file_header.header().data.offset, static_cast<u64>(input.tellp()));
  sample_event.WriteTo(&input);
  ASSERT_EQ(file_header.header().data.offset + data_size,
            static_cast<u64>(input.tellp()));

  // no metadata

  //
  // Parse input.
  //

  PerfReader pr;
  EXPECT_TRUE(pr.ReadFromString(input.str()));

  // Make sure the attr was recorded properly.
  EXPECT_EQ(1, pr.attrs().size());
  EXPECT_EQ(456, pr.attrs()[0].attr.config);

  // Verify subsequent sample event was read properly.
  ASSERT_EQ(1, pr.events().size());
  const event_t& event = *pr.events()[0].get();
  EXPECT_EQ(PERF_RECORD_SAMPLE, event.header.type);
  EXPECT_EQ(data_size, event.header.size);

  perf_sample sample_info;
  ASSERT_TRUE(pr.ReadPerfSampleInfo(event, &sample_info));
  EXPECT_EQ(0x00000000002c100a, sample_info.ip);
  EXPECT_EQ(1002, sample_info.tid);
}

// Regression test for http://crbug.com/496441
TEST(PerfReaderTest, LargePerfEventAttrPiped) {
  std::stringstream input;

  // pipe header
  testing::ExamplePipedPerfDataFileHeader().WriteTo(&input);

  // data

  // PERF_RECORD_HEADER_ATTR
  testing::ExamplePerfEventAttrEvent_Hardware(PERF_SAMPLE_IP | PERF_SAMPLE_TID,
                                              true /*sample_id_all*/)
      .WithAttrSize(sizeof(perf_event_attr) + sizeof(u64))
      .WithConfig(123)
      .WriteTo(&input);

  // PERF_RECORD_HEADER_EVENT_TYPE
  const struct event_type_event event_type = {
    .header = {
      .type = PERF_RECORD_HEADER_EVENT_TYPE,
      .misc = 0,
      .size = sizeof(struct event_type_event),
    },
    .event_type = {
      /*event_id*/ 123,
      /*name*/ "cycles",
    },
  };
  input.write(reinterpret_cast<const char*>(&event_type), sizeof(event_type));

  testing::ExamplePerfSampleEvent sample_event(
      testing::SampleInfo().Ip(0x00000000002c100a).Tid(1002));
  sample_event.WriteTo(&input);

  //
  // Parse input.
  //

  PerfReader pr;
  EXPECT_TRUE(pr.ReadFromString(input.str()));

  // Make sure the attr was recorded properly.
  EXPECT_EQ(1, pr.attrs().size());
  EXPECT_EQ(123, pr.attrs()[0].attr.config);
  EXPECT_EQ("cycles", pr.attrs()[0].name);

  // Verify subsequent sample event was read properly.
  ASSERT_EQ(1, pr.events().size());
  const event_t& event = *pr.events()[0].get();
  EXPECT_EQ(PERF_RECORD_SAMPLE, event.header.type);
  EXPECT_EQ(sample_event.GetSize(), event.header.size);

  perf_sample sample_info;
  ASSERT_TRUE(pr.ReadPerfSampleInfo(event, &sample_info));
  EXPECT_EQ(0x00000000002c100a, sample_info.ip);
  EXPECT_EQ(1002, sample_info.tid);
}

// Regression test for http://crbug.com/496441
TEST(PerfReaderTest, SmallPerfEventAttr) {
  std::stringstream input;

  const size_t attr_size = sizeof(perf_event_attr) - sizeof(u64);
  testing::ExamplePerfSampleEvent sample_event(
      testing::SampleInfo().Ip(0x00000000002c100a).Tid(1002));
  const size_t data_size = sample_event.GetSize();

  // header
  testing::ExamplePerfDataFileHeader file_header(0);
  file_header
      .WithCustomPerfEventAttrSize(attr_size)
      .WithAttrCount(1)
      .WithDataSize(data_size);
  file_header.WriteTo(&input);

  // attrs
  CHECK_EQ(file_header.header().attrs.offset,
           static_cast<u64>(input.tellp()));
  testing::ExamplePerfFileAttr_Hardware(PERF_SAMPLE_IP | PERF_SAMPLE_TID,
                                        false /*sample_id_all*/)
      .WithAttrSize(attr_size)
      .WithConfig(456)
      .WriteTo(&input);

  // data

  ASSERT_EQ(file_header.header().data.offset,
            static_cast<u64>(input.tellp()));
  sample_event.WriteTo(&input);
  ASSERT_EQ(file_header.header().data.offset + data_size,
            static_cast<u64>(input.tellp()));

  // no metadata

  //
  // Parse input.
  //

  PerfReader pr;
  EXPECT_TRUE(pr.ReadFromString(input.str()));

  // Make sure the attr was recorded properly.
  EXPECT_EQ(1, pr.attrs().size());
  EXPECT_EQ(456, pr.attrs()[0].attr.config);

  // Verify subsequent sample event was read properly.
  ASSERT_EQ(1, pr.events().size());
  const event_t& event = *pr.events()[0].get();
  EXPECT_EQ(PERF_RECORD_SAMPLE, event.header.type);
  EXPECT_EQ(data_size, event.header.size);

  perf_sample sample_info;
  ASSERT_TRUE(pr.ReadPerfSampleInfo(event, &sample_info));
  EXPECT_EQ(0x00000000002c100a, sample_info.ip);
  EXPECT_EQ(1002, sample_info.tid);
}

// Regression test for http://crbug.com/496441
TEST(PerfReaderTest, SmallPerfEventAttrPiped) {
  std::stringstream input;

  // pipe header
  testing::ExamplePipedPerfDataFileHeader().WriteTo(&input);

  // data

  // PERF_RECORD_HEADER_ATTR
  testing::ExamplePerfEventAttrEvent_Hardware(PERF_SAMPLE_IP | PERF_SAMPLE_TID,
                                              true /*sample_id_all*/)
      .WithAttrSize(sizeof(perf_event_attr) - sizeof(u64))
      .WithConfig(123)
      .WriteTo(&input);

  // PERF_RECORD_HEADER_EVENT_TYPE
  const struct event_type_event event_type = {
    .header = {
      .type = PERF_RECORD_HEADER_EVENT_TYPE,
      .misc = 0,
      .size = sizeof(struct event_type_event),
    },
    .event_type = {
      /*event_id*/ 123,
      /*name*/ "cycles",
    },
  };
  input.write(reinterpret_cast<const char*>(&event_type), sizeof(event_type));

  testing::ExamplePerfSampleEvent sample_event(
      testing::SampleInfo().Ip(0x00000000002c100a).Tid(1002));
  sample_event.WriteTo(&input);

  //
  // Parse input.
  //

  PerfReader pr;
  EXPECT_TRUE(pr.ReadFromString(input.str()));

  // Make sure the attr was recorded properly.
  EXPECT_EQ(1, pr.attrs().size());
  EXPECT_EQ(123, pr.attrs()[0].attr.config);
  EXPECT_EQ("cycles", pr.attrs()[0].name);

  // Verify subsequent sample event was read properly.
  ASSERT_EQ(1, pr.events().size());
  const event_t& event = *pr.events()[0].get();
  EXPECT_EQ(PERF_RECORD_SAMPLE, event.header.type);
  EXPECT_EQ(sample_event.GetSize(), event.header.size);

  perf_sample sample_info;
  ASSERT_TRUE(pr.ReadPerfSampleInfo(event, &sample_info));
  EXPECT_EQ(0x00000000002c100a, sample_info.ip);
  EXPECT_EQ(1002, sample_info.tid);
}

TEST(PerfReaderTest, CrossEndianAttrs) {
  for (bool is_cross_endian : {true, false}) {
    LOG(INFO) << "Testing with cross endianness = " << is_cross_endian;

    std::stringstream input;

    // header
    const uint32_t features = 0;
    testing::ExamplePerfDataFileHeader file_header(features);
    file_header
        .WithAttrCount(3)
        .WithCrossEndianness(is_cross_endian)
        .WriteTo(&input);

    // attrs
    CHECK_EQ(file_header.header().attrs.offset,
             static_cast<u64>(input.tellp()));
    // Provide two attrs with different sample_id_all values to test the
    // correctness of byte swapping of the bit fields.
    testing::ExamplePerfFileAttr_Hardware(PERF_SAMPLE_IP | PERF_SAMPLE_TID,
                                          true /*sample_id_all*/)
        .WithConfig(123)
        .WithCrossEndianness(is_cross_endian)
        .WriteTo(&input);
    testing::ExamplePerfFileAttr_Hardware(PERF_SAMPLE_IP | PERF_SAMPLE_TID,
                                          true /*sample_id_all*/)
        .WithConfig(456)
        .WithCrossEndianness(is_cross_endian)
        .WriteTo(&input);
    testing::ExamplePerfFileAttr_Hardware(PERF_SAMPLE_IP | PERF_SAMPLE_TID,
                                          false /*sample_id_all*/)
        .WithConfig(456)
        .WithCrossEndianness(is_cross_endian)
        .WriteTo(&input);

    // No data.
    // No metadata.

    // Read data.

    PerfReader pr;
    EXPECT_TRUE(pr.ReadFromString(input.str()));

    // Make sure the attr was recorded properly.
    EXPECT_EQ(3, pr.attrs().size());

    const perf_event_attr& attr0 = pr.attrs()[0].attr;
    EXPECT_EQ(123, attr0.config);
    EXPECT_EQ(1, attr0.sample_period);
    EXPECT_EQ(PERF_SAMPLE_IP | PERF_SAMPLE_TID, attr0.sample_type);
    EXPECT_TRUE(attr0.sample_id_all);
    EXPECT_EQ(2, attr0.precise_ip);

    const perf_event_attr& attr1 = pr.attrs()[1].attr;
    EXPECT_EQ(456, attr1.config);
    EXPECT_EQ(1, attr1.sample_period);
    EXPECT_EQ(PERF_SAMPLE_IP | PERF_SAMPLE_TID, attr1.sample_type);
    EXPECT_TRUE(attr1.sample_id_all);
    EXPECT_EQ(2, attr1.precise_ip);

    const perf_event_attr& attr2 = pr.attrs()[2].attr;
    EXPECT_EQ(456, attr2.config);
    EXPECT_EQ(1, attr2.sample_period);
    EXPECT_EQ(PERF_SAMPLE_IP | PERF_SAMPLE_TID, attr2.sample_type);
    EXPECT_FALSE(attr2.sample_id_all);
    EXPECT_EQ(2, attr2.precise_ip);
  }
}

TEST(PerfReaderTest, CrossEndianNormalPerfData) {
  // data
  // Do this before header to compute the total data size.
  std::stringstream input_data;
  testing::ExampleMmapEvent(
      1234, 0x0000000000810000, 0x10000, 0x2000, "/usr/lib/foo.so",
      testing::SampleInfo().Tid(bswap_32(1234), bswap_32(1235)))
      .WithCrossEndianness(true)
      .WriteTo(&input_data);
  testing::ExampleForkEvent(
      1236, 1234, 1237, 1235, 30ULL * 1000000000,
      testing::SampleInfo().Tid(bswap_32(1236), bswap_32(1237)))
      .WithCrossEndianness(true)
      .WriteTo(&input_data);
  testing::ExamplePerfSampleEvent(
      testing::SampleInfo()
          .Ip(bswap_64(0x0000000000810100))
          .Tid(bswap_32(1234), bswap_32(1235)))
      .WithCrossEndianness(true)
      .WriteTo(&input_data);
  testing::ExamplePerfSampleEvent(
      testing::SampleInfo()
          .Ip(bswap_64(0x000000000081ff00))
          .Tid(bswap_32(1236), bswap_32(1237)))
      .WithCrossEndianness(true)
      .WriteTo(&input_data);

  std::stringstream input;

  // header
  const size_t data_size = input_data.str().size();
  const uint32_t features = (1 << HEADER_HOSTNAME) | (1 << HEADER_OSRELEASE);
  testing::ExamplePerfDataFileHeader file_header(features);
  file_header
      .WithAttrCount(1)
      .WithDataSize(data_size)
      .WithCrossEndianness(true)
      .WriteTo(&input);

  // attrs
  CHECK_EQ(file_header.header().attrs.offset,
           static_cast<u64>(input.tellp()));
  testing::ExamplePerfFileAttr_Hardware(PERF_SAMPLE_IP | PERF_SAMPLE_TID,
                                        true /*sample_id_all*/)
      .WithConfig(456)
      .WithCrossEndianness(true)
      .WriteTo(&input);

  // Write data.

  u64 data_offset = file_header.header().data.offset;
  ASSERT_EQ(data_offset, static_cast<u64>(input.tellp()));
  input << input_data.str();
  ASSERT_EQ(data_offset + data_size, static_cast<u64>(input.tellp()));

  // metadata
  size_t metadata_offset =
      file_header.data_end() + 2 * sizeof(perf_file_section);

  // HEADER_HOSTNAME
  testing::ExampleStringMetadata hostname_metadata("hostname", metadata_offset);
  hostname_metadata.WithCrossEndianness(true);
  metadata_offset += hostname_metadata.size();

  // HEADER_OSRELEASE
  testing::ExampleStringMetadata osrelease_metadata("osrelease",
                                                    metadata_offset);
  osrelease_metadata.WithCrossEndianness(true);
  metadata_offset += osrelease_metadata.size();

  hostname_metadata.index_entry().WriteTo(&input);
  osrelease_metadata.index_entry().WriteTo(&input);

  hostname_metadata.WriteTo(&input);
  osrelease_metadata.WriteTo(&input);


  //
  // Parse input.
  //

  PerfReader pr;
  EXPECT_TRUE(pr.ReadFromString(input.str()));

  // Make sure the attr was recorded properly.
  EXPECT_EQ(1, pr.attrs().size());
  EXPECT_EQ(456, pr.attrs()[0].attr.config);
  EXPECT_TRUE(pr.attrs()[0].attr.sample_id_all);

  // Verify perf events.
  ASSERT_EQ(4, pr.events().size());

  {
    const event_t& event = *pr.events()[0].get();
    EXPECT_EQ(PERF_RECORD_MMAP, event.header.type);
    EXPECT_EQ(1234, event.mmap.pid);
    EXPECT_EQ(1234, event.mmap.tid);
    EXPECT_EQ(string("/usr/lib/foo.so"), event.mmap.filename);
    EXPECT_EQ(0x0000000000810000, event.mmap.start);
    EXPECT_EQ(0x10000, event.mmap.len);
    EXPECT_EQ(0x2000, event.mmap.pgoff);
  }

  {
    const event_t& event = *pr.events()[1].get();
    EXPECT_EQ(PERF_RECORD_FORK, event.header.type);
    EXPECT_EQ(1236, event.fork.pid);
    EXPECT_EQ(1234, event.fork.ppid);
    EXPECT_EQ(1237, event.fork.tid);
    EXPECT_EQ(1235, event.fork.ptid);
    EXPECT_EQ(30ULL * 1000000000, event.fork.time);
  }

  {
    const event_t& event = *pr.events()[2].get();
    EXPECT_EQ(PERF_RECORD_SAMPLE, event.header.type);

    perf_sample sample_info;
    ASSERT_TRUE(pr.ReadPerfSampleInfo(event, &sample_info));
    EXPECT_EQ(0x0000000000810100, sample_info.ip);
    EXPECT_EQ(1234, sample_info.pid);
    EXPECT_EQ(1235, sample_info.tid);
  }

  {
    const event_t& event = *pr.events()[3].get();
    EXPECT_EQ(PERF_RECORD_SAMPLE, event.header.type);

    perf_sample sample_info;
    ASSERT_TRUE(pr.ReadPerfSampleInfo(event, &sample_info));
    EXPECT_EQ(0x000000000081ff00, sample_info.ip);
    EXPECT_EQ(1236, sample_info.pid);
    EXPECT_EQ(1237, sample_info.tid);
  }
}

}  // namespace quipper

int main(int argc, char* argv[]) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
