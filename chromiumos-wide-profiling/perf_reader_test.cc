// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>

#include <set>
#include <string>

#include "base/logging.h"

#include "perf_reader.h"
#include "perf_test_files.h"
#include "quipper_string.h"
#include "utils.h"

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

// Check file names.  GetFilenamesToBuildIDs is tested with InjectBuildIDs and
// Localize in perf_parser_test.
void CheckFilenames(const PerfReader& pr) {
  std::vector<string> filenames;
  pr.GetFilenames(&filenames);
  CheckNoDuplicates(filenames);

  for (size_t i = 0; i < arraysize(kExpectedFilenameSubstrings); ++i)
    CheckForElementWithSubstring(kExpectedFilenameSubstrings[i], filenames);

  std::set<string> filename_set;
  pr.GetFilenamesAsSet(&filename_set);

  std::vector<PerfEventAndSampleInfo> events = pr.events();
  for (size_t i = 0; i < events.size(); ++i) {
    const event_t& event = events[i].event;
    if (event.header.type == PERF_RECORD_MMAP) {
      EXPECT_TRUE(filename_set.find(event.mmap.filename) != filename_set.end())
          << event.mmap.filename << " is not present in the filename set";
    }
  }
}

}  // namespace

TEST(PerfReaderTest, Test1Cycle) {
  for (unsigned int i = 0;
       i < arraysize(perf_test_files::kPerfDataFiles);
       ++i) {
    PerfReader pr;
    string input_perf_data = perf_test_files::kPerfDataFiles[i];
    LOG(INFO) << "Testing " << input_perf_data;
    string output_perf_data = input_perf_data + ".pr.out";
    ASSERT_TRUE(pr.ReadFile(input_perf_data));
    CheckFilenames(pr);
    ASSERT_TRUE(pr.WriteFile(output_perf_data));

    EXPECT_TRUE(ComparePerfReports(input_perf_data, output_perf_data));
    EXPECT_TRUE(ComparePerfBuildIDLists(input_perf_data, output_perf_data));
  }

  std::set<string> metadata;
  for (unsigned int i = 0;
       i < arraysize(perf_test_files::kPerfPipedDataFiles);
       ++i) {
    PerfReader pr;
    string input_perf_data = perf_test_files::kPerfPipedDataFiles[i];
    LOG(INFO) << "Testing " << input_perf_data;
    string output_perf_data = input_perf_data + ".pr.out";
    ASSERT_TRUE(pr.ReadFile(input_perf_data));
    CheckFilenames(pr);
    ASSERT_TRUE(pr.WriteFile(output_perf_data));

    EXPECT_TRUE(ComparePipedPerfReports(input_perf_data, output_perf_data,
                                        &metadata));
  }

  // For piped data, perf report doesn't check metadata.
  // Make sure that each metadata type is seen at least once.
  for (size_t i = 0; kSupportedMetadata[i]; ++i) {
    EXPECT_NE(metadata.find(kSupportedMetadata[i]), metadata.end())
        << "No output file from piped input files had "
        << kSupportedMetadata[i] << " metadata";
  }
}

}  // namespace quipper

int main(int argc, char* argv[]) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
