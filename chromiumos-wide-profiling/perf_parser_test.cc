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

void CheckMembership(string element_to_find, const std::vector<string>& list) {
  std::vector<string>::const_iterator iter;
  for (iter = list.begin(); iter != list.end(); ++iter)
    if (element_to_find == *iter)
      return;
  ADD_FAILURE() << element_to_find << " is not present in the given list";
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

void CheckNoDuplicates(const std::vector<string>& list) {
  std::set<string> list_as_set(list.begin(), list.end());
  if (list.size() != list_as_set.size())
    ADD_FAILURE() << "Given list has at least one duplicate";
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

  // Check file names.
  std::vector<string> filenames;
  parser->GetFilenames(&filenames);
  CheckNoDuplicates(filenames);

  // Any run of perf should have MMAPs with the following substrings:
  CheckForElementWithSubstring("perf", filenames);
  CheckForElementWithSubstring("kernel", filenames);
  CheckForElementWithSubstring(".ko", filenames);
  CheckForElementWithSubstring("libc", filenames);

  std::vector<ParsedEvent*> parsed_events = parser->GetEventsSortedByTime();
  for (size_t i = 0; i < parsed_events.size(); i++) {
    const event_t& event = *(parsed_events[i]->raw_event);
    if (event.header.type == PERF_RECORD_MMAP)
      CheckMembership(event.mmap.filename, filenames);
  }
}

void CreateFilenameToBuildIDMap(
    const std::vector<string>& filenames, unsigned int seed,
    std::map<string, string>* filenames_to_build_ids) {
  srand(seed);
  for (size_t i = 0; i < filenames.size(); ++i) {
    u8 build_id[kBuildIDArraySize];
    for (size_t j = 0; j < kBuildIDArraySize; ++j)
      build_id[j] = rand_r(&seed);

    (*filenames_to_build_ids)[filenames[i]] =
        HexToString(build_id, kBuildIDArraySize);
  }
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

TEST(PerfParserTest, TestProcessing) {
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
  }
}

TEST(PerfParserTest, TestBuildIDInjection) {
  bool injected_at_least_once = false;
  for (unsigned int i = 0;
       i < arraysize(perf_test_files::kPerfPipedDataFiles);
       ++i) {
    string input_perf_data = perf_test_files::kPerfPipedDataFiles[i];
    LOG(INFO) << "Testing " << input_perf_data;

    PerfParser parser;
    ReadFileAndCheckInternals(input_perf_data, &parser);

    if (!parser.build_id_events().empty())
      continue;

    std::vector<string> filenames;
    parser.GetFilenames(&filenames);
    ASSERT_FALSE(filenames.empty());

    std::map<string, string> filenames_to_build_ids;
    CreateFilenameToBuildIDMap(filenames, i, &filenames_to_build_ids);
    ASSERT_TRUE(parser.InjectBuildIDs(filenames_to_build_ids));
    injected_at_least_once = true;

    string output_perf_data = input_perf_data + ".parse.remap.out";
    ASSERT_TRUE(parser.WriteFile(output_perf_data));

    std::map<string, string> perf_build_id_map;
    ASSERT_TRUE(GetPerfBuildIDMap(output_perf_data, &perf_build_id_map));
    EXPECT_EQ(filenames_to_build_ids, perf_build_id_map);

    std::map<string, string> filename_localizer;
    // Only localize some files, and skip some.
    // One simple way is to replace indexes at powers of 2.
    for (size_t j = 1; j < filenames.size(); j = j << 1) {
      string old_filename = filenames[j];
      string new_filename = old_filename + ".local";
      filenames[j] = new_filename;

      string build_id = filenames_to_build_ids[old_filename];
      filename_localizer[build_id] = new_filename;
      filenames_to_build_ids[new_filename] = build_id;
      filenames_to_build_ids.erase(old_filename);
    }

    // Add a build id that is too short and make sure it doesn't break things
    filename_localizer["fecba9876543210"] = "hello_world.cc";

    parser.Localize(filename_localizer);
    std::vector<string> new_filenames;
    parser.GetFilenames(&new_filenames);
    std::sort(filenames.begin(), filenames.end());
    EXPECT_EQ(filenames, new_filenames);

    string output_perf_data2 = input_perf_data + ".parse.remap2.out";
    ASSERT_TRUE(parser.WriteFile(output_perf_data2));

    perf_build_id_map.clear();
    ASSERT_TRUE(GetPerfBuildIDMap(output_perf_data2, &perf_build_id_map));
    EXPECT_EQ(filenames_to_build_ids, perf_build_id_map);
  }

  EXPECT_TRUE(injected_at_least_once);
}

}  // namespace quipper

int main(int argc, char* argv[]) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
