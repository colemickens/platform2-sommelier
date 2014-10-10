// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "base/logging.h"

#include "chromiumos-wide-profiling/perf_reader.h"
#include "chromiumos-wide-profiling/perf_test_files.h"
#include "chromiumos-wide-profiling/quipper_string.h"
#include "chromiumos-wide-profiling/quipper_test.h"
#include "chromiumos-wide-profiling/scoped_temp_path.h"
#include "chromiumos-wide-profiling/test_perf_data.h"
#include "chromiumos-wide-profiling/test_utils.h"
#include "chromiumos-wide-profiling/utils.h"

namespace quipper {

namespace {

// Any run of perf should have MMAPs with the following substrings.
const char* kExpectedFilenameSubstrings[] = {
  "perf",
  "kernel",
  "libc",
};

void CheckNoDuplicates(const std::vector<string>& list) {
  std::set<string> list_as_set(list.begin(), list.end());
  if (list.size() != list_as_set.size())
    ADD_FAILURE() << "Given list has at least one duplicate";
}

void CheckForElementWithSubstring(string substring_to_find,
                                  const std::vector<string>& list) {
  std::vector<string>::const_iterator iter;
  for (iter = list.begin(); iter != list.end(); ++iter)
    if (iter->find(substring_to_find) != string::npos)
      return;
  ADD_FAILURE() << substring_to_find
                << " is not present in any of the elements of the given list";
}

void CreateFilenameToBuildIDMap(
    const std::vector<string>& filenames, unsigned int seed,
    std::map<string, string>* filenames_to_build_ids) {
  srand(seed);
  // Only use every other filename, so that half the filenames are unused.
  for (size_t i = 0; i < filenames.size(); i += 2) {
    u8 build_id[kBuildIDArraySize];
    for (size_t j = 0; j < kBuildIDArraySize; ++j)
      build_id[j] = rand_r(&seed);

    (*filenames_to_build_ids)[filenames[i]] =
        HexToString(build_id, kBuildIDArraySize);
  }
}

void CheckFilenameAndBuildIDMethods(const string& input_perf_data,
                                    const string& output_perf_data_prefix,
                                    unsigned int seed,
                                    PerfReader* reader) {
  // Check filenames.
  std::vector<string> filenames;
  reader->GetFilenames(&filenames);

  ASSERT_FALSE(filenames.empty());
  CheckNoDuplicates(filenames);
  for (size_t i = 0; i < arraysize(kExpectedFilenameSubstrings); ++i)
    CheckForElementWithSubstring(kExpectedFilenameSubstrings[i], filenames);

  std::set<string> filename_set;
  reader->GetFilenamesAsSet(&filename_set);

  // Make sure all MMAP filenames are in the set.
  for (const auto& event : reader->events()) {
    if (event->header.type == PERF_RECORD_MMAP) {
      EXPECT_TRUE(filename_set.find(event->mmap.filename) != filename_set.end())
          << event->mmap.filename << " is not present in the filename set";
    }
  }

  std::map<string, string> expected_map;
  reader->GetFilenamesToBuildIDs(&expected_map);

  // Inject some made up build ids.
  std::map<string, string> filenames_to_build_ids;
  CreateFilenameToBuildIDMap(filenames, seed, &filenames_to_build_ids);
  ASSERT_TRUE(reader->InjectBuildIDs(filenames_to_build_ids));

  // Reader should now correctly populate the filenames to build ids map.
  std::map<string, string>::const_iterator it;
  for (it = filenames_to_build_ids.begin();
       it != filenames_to_build_ids.end();
       ++it) {
    expected_map[it->first] = it->second;
  }
  std::map<string, string> reader_map;
  reader->GetFilenamesToBuildIDs(&reader_map);
  EXPECT_EQ(expected_map, reader_map);

  string output_perf_data1 = output_perf_data_prefix + ".parse.inject.out";
  ASSERT_TRUE(reader->WriteFile(output_perf_data1));

  // Perf should find the same build ids.
  std::map<string, string> perf_build_id_map;
  ASSERT_TRUE(GetPerfBuildIDMap(output_perf_data1, &perf_build_id_map));
  EXPECT_EQ(expected_map, perf_build_id_map);

  std::map<string, string> build_id_localizer;
  // Only localize the first half of the files which have build ids.
  for (size_t j = 0; j < filenames.size() / 2; ++j) {
    string old_filename = filenames[j];
    if (expected_map.find(old_filename) == expected_map.end())
      continue;
    string build_id = expected_map[old_filename];

    string new_filename = old_filename + ".local";
    filenames[j] = new_filename;
    build_id_localizer[build_id] = new_filename;
    expected_map[new_filename] = build_id;
    expected_map.erase(old_filename);
  }
  reader->Localize(build_id_localizer);

  // Filenames should be the same.
  std::vector<string> new_filenames;
  reader->GetFilenames(&new_filenames);
  std::sort(filenames.begin(), filenames.end());
  EXPECT_EQ(filenames, new_filenames);

  // Build ids should be updated.
  reader_map.clear();
  reader->GetFilenamesToBuildIDs(&reader_map);
  EXPECT_EQ(expected_map, reader_map);

  string output_perf_data2 = output_perf_data_prefix + ".parse.localize.out";
  ASSERT_TRUE(reader->WriteFile(output_perf_data2));

  perf_build_id_map.clear();
  ASSERT_TRUE(GetPerfBuildIDMap(output_perf_data2, &perf_build_id_map));
  EXPECT_EQ(expected_map, perf_build_id_map);

  std::map<string, string> filename_localizer;
  // Only localize every third filename.
  for (size_t j = 0; j < filenames.size(); j += 3) {
    string old_filename = filenames[j];
    string new_filename = old_filename + ".local2";
    filenames[j] = new_filename;
    filename_localizer[old_filename] = new_filename;

    if (expected_map.find(old_filename) != expected_map.end()) {
      string build_id = expected_map[old_filename];
      expected_map[new_filename] = build_id;
      expected_map.erase(old_filename);
    }
  }
  reader->LocalizeUsingFilenames(filename_localizer);

  // Filenames should be the same.
  new_filenames.clear();
  reader->GetFilenames(&new_filenames);
  std::sort(filenames.begin(), filenames.end());
  EXPECT_EQ(filenames, new_filenames);

  // Build ids should be updated.
  reader_map.clear();
  reader->GetFilenamesToBuildIDs(&reader_map);
  EXPECT_EQ(expected_map, reader_map);

  string output_perf_data3 = output_perf_data_prefix + ".parse.localize2.out";
  ASSERT_TRUE(reader->WriteFile(output_perf_data3));

  perf_build_id_map.clear();
  ASSERT_TRUE(GetPerfBuildIDMap(output_perf_data3, &perf_build_id_map));
  EXPECT_EQ(expected_map, perf_build_id_map);
}

}  // namespace

TEST(PerfReaderTest, NormalModePerfData) {
  ScopedTempDir output_dir;
  ASSERT_FALSE(output_dir.path().empty());
  string output_path = output_dir.path();

  int seed = 0;
  for (const char* test_file : perf_test_files::kPerfDataFiles) {
    string input_perf_data = GetTestInputFilePath(test_file);
    LOG(INFO) << "Testing " << input_perf_data;
    string output_perf_data = output_path + test_file + ".pr.out";
    PerfReader pr;
    ASSERT_TRUE(pr.ReadFile(input_perf_data));
    ASSERT_TRUE(pr.WriteFile(output_perf_data));

    EXPECT_TRUE(CheckPerfDataAgainstBaseline(input_perf_data));
    EXPECT_TRUE(CheckPerfDataAgainstBaseline(output_perf_data));
    EXPECT_TRUE(ComparePerfBuildIDLists(input_perf_data, output_perf_data));
    CheckFilenameAndBuildIDMethods(input_perf_data, output_path + test_file,
                                   seed, &pr);
    ++seed;
  }
}

TEST(PerfReaderTest, PipedModePerfData) {
  ScopedTempDir output_dir;
  ASSERT_FALSE(output_dir.path().empty());
  string output_path = output_dir.path();

  int seed = 0;
  for (const char* test_file : perf_test_files::kPerfPipedDataFiles) {
    string input_perf_data = GetTestInputFilePath(test_file);
    LOG(INFO) << "Testing " << input_perf_data;
    string output_perf_data = output_path + test_file + ".pr.out";
    PerfReader pr;
    ASSERT_TRUE(pr.ReadFile(input_perf_data));
    ASSERT_TRUE(pr.WriteFile(output_perf_data));

    EXPECT_TRUE(CheckPerfDataAgainstBaseline(input_perf_data));
    EXPECT_TRUE(CheckPerfDataAgainstBaseline(output_perf_data));
    CheckFilenameAndBuildIDMethods(input_perf_data, output_path + test_file,
                                   seed, &pr);
    ++seed;
  }
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
  string test = "f";
  PerfReader::PerfizeBuildIDString(&test);
  EXPECT_EQ("f000000000000000000000000000000000000000", test);
  PerfReader::PerfizeBuildIDString(&test);
  EXPECT_EQ("f000000000000000000000000000000000000000", test);

  test = "01234567890123456789012345678901234567890";
  PerfReader::PerfizeBuildIDString(&test);
  EXPECT_EQ("0123456789012345678901234567890123456789", test);
  PerfReader::PerfizeBuildIDString(&test);
  EXPECT_EQ("0123456789012345678901234567890123456789", test);
}

TEST(PerfReaderTest, UnperfizeBuildID) {
  string test = "f000000000000000000000000000000000000000";
  PerfReader::UnperfizeBuildIDString(&test);
  EXPECT_EQ("f0000000", test);
  PerfReader::UnperfizeBuildIDString(&test);
  EXPECT_EQ("f0000000", test);

  test = "0123456789012345678901234567890123456789";
  PerfReader::UnperfizeBuildIDString(&test);
  EXPECT_EQ("0123456789012345678901234567890123456789", test);

  test = "0000000000000000000000000000000000000000";
  PerfReader::UnperfizeBuildIDString(&test);
  EXPECT_EQ("00000000", test);
  PerfReader::UnperfizeBuildIDString(&test);
  EXPECT_EQ("00000000", test);

  test = "0000000000000000000000000000001000000000";
  PerfReader::UnperfizeBuildIDString(&test);
  EXPECT_EQ("00000000000000000000000000000010", test);
  PerfReader::UnperfizeBuildIDString(&test);
  EXPECT_EQ("00000000000000000000000000000010", test);
}

TEST(PerfReaderTest, ReadsTraceMetadata) {
  std::stringstream input;

  const size_t attr_count = 1;

  // header
  testing::ExamplePerfDataFileHeader file_header(
      attr_count, 1 << HEADER_TRACING_DATA);
  file_header.WriteTo(&input);
  const perf_file_header &header = file_header.header();

  // attrs
  testing::ExamplePerfFileAttr_Tracepoint(73).WriteTo(&input);

  // data
  ASSERT_EQ(static_cast<u64>(input.tellp()), header.data.offset);
  testing::ExamplePerfSampleEvent_Tracepoint().WriteTo(&input);
  ASSERT_EQ(input.tellp(), file_header.data_end());

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

  // header

  const perf_pipe_file_header header = {
    .magic = kPerfMagic,
    .size = 16,
  };
  input.write(reinterpret_cast<const char*>(&header), sizeof(header));
  ASSERT_EQ(input.tellp(), header.size);

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
}

}  // namespace quipper

int main(int argc, char* argv[]) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
