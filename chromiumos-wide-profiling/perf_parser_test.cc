// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>

#include <algorithm>
#include <map>
#include <set>
#include <string>

#include "base/logging.h"

#include "perf_parser.h"
#include "perf_reader.h"
#include "perf_test_files.h"
#include "quipper_string.h"
#include "utils.h"

namespace quipper {

namespace {

void CheckChronologicalOrderOfEvents(const std::vector<ParsedEvent*>& events) {
  for (unsigned int i = 1; i < events.size(); ++i) {
    uint64 time = events[i]->sample_info->time;
    uint64 prev_time = events[i - 1]->sample_info->time;
    CHECK_LE(prev_time, time);
  }
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

void ReadFileAndCheckInternals(const string& input_perf_data,
                               PerfParser* parser) {
  parser->set_do_remap(true);
  ASSERT_TRUE(parser->ReadFile(input_perf_data));
  parser->ParseRawEvents();

  // Check perf event stats.
  const PerfEventStats& stats = parser->stats();
  EXPECT_GT(stats.num_sample_events, 0U);
  EXPECT_GT(stats.num_mmap_events, 0U);
  EXPECT_GT(stats.num_sample_events_mapped, 0U);
  EXPECT_TRUE(stats.did_remap);
}

void CheckInjectionAndLocalization(const string& input_perf_data,
                                   unsigned int seed,
                                   PerfParser* parser) {
    std::vector<string> filenames;
    parser->GetFilenames(&filenames);
    ASSERT_FALSE(filenames.empty());

    std::map<string, string> expected_map;
    parser->GetFilenamesToBuildIDs(&expected_map);

    // Inject some made up build ids.
    std::map<string, string> filenames_to_build_ids;
    CreateFilenameToBuildIDMap(filenames, seed, &filenames_to_build_ids);
    ASSERT_TRUE(parser->InjectBuildIDs(filenames_to_build_ids));

    // Parser should now correctly populate the filenames to build ids map.
    std::map<string, string>::const_iterator it;
    for (it = filenames_to_build_ids.begin();
         it != filenames_to_build_ids.end();
         ++it) {
      expected_map[it->first] = it->second;
    }
    std::map<string, string> parser_map;
    parser->GetFilenamesToBuildIDs(&parser_map);
    EXPECT_EQ(expected_map, parser_map);

    string output_perf_data = input_perf_data + ".parse.inject.out";
    ASSERT_TRUE(parser->WriteFile(output_perf_data));

    // Perf should find the same build ids.
    std::map<string, string> perf_build_id_map;
    ASSERT_TRUE(GetPerfBuildIDMap(output_perf_data, &perf_build_id_map));
    EXPECT_EQ(expected_map, perf_build_id_map);

    std::map<string, string> filename_localizer;
    // Only localize the first half of the files which have build ids.
    for (size_t j = 0; j < filenames.size() / 2; ++j) {
      string old_filename = filenames[j];
      if (expected_map.find(old_filename) == expected_map.end())
        continue;
      string build_id = expected_map[old_filename];

      string new_filename = old_filename + ".local";
      filenames[j] = new_filename;
      filename_localizer[build_id] = new_filename;
      expected_map[new_filename] = build_id;
      expected_map.erase(old_filename);
    }
    parser->Localize(filename_localizer);

    // Filenames should be the same.
    std::vector<string> new_filenames;
    parser->GetFilenames(&new_filenames);
    std::sort(filenames.begin(), filenames.end());
    EXPECT_EQ(filenames, new_filenames);

    // Build ids should be updated.
    parser_map.clear();
    parser->GetFilenamesToBuildIDs(&parser_map);
    EXPECT_EQ(expected_map, parser_map);

    string output_perf_data2 = input_perf_data + ".parse.localize.out";
    ASSERT_TRUE(parser->WriteFile(output_perf_data2));

    perf_build_id_map.clear();
    ASSERT_TRUE(GetPerfBuildIDMap(output_perf_data2, &perf_build_id_map));
    EXPECT_EQ(expected_map, perf_build_id_map);
}

}  // namespace

TEST(PerfParserTest, Test1Cycle) {
  for (unsigned int i = 0;
       i < arraysize(perf_test_files::kPerfDataFiles);
       ++i) {
    string input_perf_data = perf_test_files::kPerfDataFiles[i];
    LOG(INFO) << "Testing " << input_perf_data;

    PerfParser parser;
    ASSERT_TRUE(parser.ReadFile(input_perf_data));
    parser.ParseRawEvents();

    CHECK_GT(parser.GetEventsSortedByTime().size(), 0U);
    CheckChronologicalOrderOfEvents(parser.GetEventsSortedByTime());

    // Check perf event stats.
    const PerfEventStats& stats = parser.stats();
    EXPECT_GT(stats.num_sample_events, 0U);
    EXPECT_GT(stats.num_mmap_events, 0U);
    EXPECT_GT(stats.num_sample_events_mapped, 0U);
    EXPECT_FALSE(stats.did_remap);

    string output_perf_data = input_perf_data + ".parse.out";
    ASSERT_TRUE(parser.WriteFile(output_perf_data));

    EXPECT_TRUE(ComparePerfReports(input_perf_data, output_perf_data));
    EXPECT_TRUE(ComparePerfBuildIDLists(input_perf_data, output_perf_data));
  }
}

TEST(PerfParserTest, TestNormalProcessing) {
  for (unsigned int i = 0;
       i < arraysize(perf_test_files::kPerfDataFiles);
       ++i) {
    string input_perf_data = perf_test_files::kPerfDataFiles[i];
    LOG(INFO) << "Testing " << input_perf_data;

    PerfParser parser;
    ReadFileAndCheckInternals(input_perf_data, &parser);

    string output_perf_data = input_perf_data + ".parse.remap.out";
    ASSERT_TRUE(parser.WriteFile(output_perf_data));

    // Remapped addresses should not match the original addresses.
    EXPECT_FALSE(ComparePerfReports(input_perf_data, output_perf_data));

    PerfParser remap_parser;
    ReadFileAndCheckInternals(output_perf_data, &remap_parser);
    string output_perf_data2 = input_perf_data + ".parse.remap2.out";
    ASSERT_TRUE(remap_parser.WriteFile(output_perf_data2));

    // Remapping again should produce the same addresses.
    EXPECT_TRUE(ComparePerfReports(output_perf_data, output_perf_data2));
    EXPECT_TRUE(ComparePerfBuildIDLists(output_perf_data, output_perf_data2));

    CheckInjectionAndLocalization(input_perf_data, i, &parser);
  }
}

TEST(PerfParserTest, TestPipedProcessing) {
  for (unsigned int i = 0;
       i < arraysize(perf_test_files::kPerfPipedDataFiles);
       ++i) {
    string input_perf_data = perf_test_files::kPerfPipedDataFiles[i];
    LOG(INFO) << "Testing " << input_perf_data;

    PerfParser parser;
    ReadFileAndCheckInternals(input_perf_data, &parser);
    CheckInjectionAndLocalization(input_perf_data, i, &parser);
  }
}

}  // namespace quipper

int main(int argc, char* argv[]) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
