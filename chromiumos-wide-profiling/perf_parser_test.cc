// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <algorithm>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/logging.h"

#include "chromiumos-wide-profiling/compat/string.h"
#include "chromiumos-wide-profiling/compat/test.h"
#include "chromiumos-wide-profiling/perf_parser.h"
#include "chromiumos-wide-profiling/perf_reader.h"
#include "chromiumos-wide-profiling/perf_test_files.h"
#include "chromiumos-wide-profiling/scoped_temp_path.h"
#include "chromiumos-wide-profiling/test_perf_data.h"
#include "chromiumos-wide-profiling/test_utils.h"

namespace quipper {

namespace {

void CheckChronologicalOrderOfEvents(const PerfReader& reader,
                                     const std::vector<ParsedEvent*>& events) {
  // Here a valid PerfReader is needed to read the sample info because
  // the attr for an event needs to be looked up in ReadPerfSampleInfo()
  // to determine which sample info fields are present.
  struct perf_sample sample_info;
  CHECK(reader.ReadPerfSampleInfo(*events[0]->raw_event, &sample_info));
  uint64_t prev_time = sample_info.time;
  for (unsigned int i = 1; i < events.size(); ++i) {
    struct perf_sample sample_info;
    CHECK(reader.ReadPerfSampleInfo(*events[i]->raw_event, &sample_info));
    CHECK_LE(prev_time, sample_info.time);
    prev_time = sample_info.time;
  }
}

}  // namespace

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
    const std::vector<string>& filenames,
    unsigned int seed,
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

// Given a PerfReader that has already consumed an input perf data file, inject
// new build IDs for the MMAP'd files in the perf data and check that they have
// been correctly injected.
void CheckFilenameAndBuildIDMethods(PerfReader* reader,
                                    const string& output_perf_data_prefix,
                                    unsigned int seed) {
  // Check filenames.
  std::vector<string> filenames;
  reader->GetFilenames(&filenames);

  ASSERT_FALSE(filenames.empty());
  CheckNoDuplicates(filenames);
  for (const char* substring : kExpectedFilenameSubstrings)
    CheckForElementWithSubstring(substring, filenames);

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

TEST(PerfParserTest, TestDSOAndOffsetConstructor) {
  // DSOAndOffset contains a pointer to a dso info struct. Make sure this is
  // initialized in a way such that DSOAndOffset::dso_name() executes without
  // segfault and returns an empty string.
  ParsedEvent::DSOAndOffset dso_and_offset;
  EXPECT_TRUE(dso_and_offset.dso_name().empty());
}

TEST(PerfParserTest, NormalPerfData) {
  ScopedTempDir output_dir;
  ASSERT_FALSE(output_dir.path().empty());
  string output_path = output_dir.path();

  int seed = 0;
  for (const char* test_file : perf_test_files::GetPerfDataFiles()) {
    string input_perf_data = GetTestInputFilePath(test_file);
    LOG(INFO) << "Testing " << input_perf_data;

    PerfReader reader;
    ASSERT_TRUE(reader.ReadFile(input_perf_data));

    PerfParser parser(&reader, GetTestOptions());
    ASSERT_TRUE(parser.ParseRawEvents());

    // Test the PerfReader stage of the processing before continuing.
    string pr_output_perf_data = output_path + test_file + ".pr.out";
    ASSERT_TRUE(reader.WriteFile(pr_output_perf_data));
    EXPECT_TRUE(CheckPerfDataAgainstBaseline(pr_output_perf_data));

//    parser.ParseRawEvents(&reader);
    CHECK_GT(parser.GetEventsSortedByTime().size(), 0U);
    CheckChronologicalOrderOfEvents(reader, parser.GetEventsSortedByTime());

    // Check perf event stats.
    const PerfEventStats& stats = parser.stats();
    EXPECT_GT(stats.num_sample_events, 0U);
    EXPECT_GT(stats.num_mmap_events, 0U);
    EXPECT_GT(stats.num_sample_events_mapped, 0U);
    EXPECT_FALSE(stats.did_remap);

    string parsed_perf_data = output_path + test_file + ".parse.out";
    ASSERT_TRUE(reader.WriteFile(parsed_perf_data));

    EXPECT_TRUE(CheckPerfDataAgainstBaseline(parsed_perf_data));
    EXPECT_TRUE(ComparePerfBuildIDLists(input_perf_data, parsed_perf_data));

    // Run the event parsing again, this time with remapping.
    PerfParserOptions options;
    options.do_remap = true;
    parser.set_options(options);
    ASSERT_TRUE(parser.ParseRawEvents());

    // Check perf event stats.
    EXPECT_GT(stats.num_sample_events, 0U);
    EXPECT_GT(stats.num_mmap_events, 0U);
    EXPECT_GT(stats.num_sample_events_mapped, 0U);
    EXPECT_TRUE(stats.did_remap);

    // Remapped addresses should not match the original addresses.
    string remapped_perf_data = output_path + test_file + ".parse.remap.out";
    ASSERT_TRUE(reader.WriteFile(remapped_perf_data));
    EXPECT_TRUE(CheckPerfDataAgainstBaseline(remapped_perf_data));

    // Remapping again should produce the same addresses.
    LOG(INFO) << "Reading in remapped data: " << remapped_perf_data;
    PerfReader remap_reader;
    ASSERT_TRUE(remap_reader.ReadFile(remapped_perf_data));

    PerfParser remap_parser(&remap_reader, options);
    ASSERT_TRUE(remap_parser.ParseRawEvents());

    const PerfEventStats& remap_stats = remap_parser.stats();
    EXPECT_GT(remap_stats.num_sample_events, 0U);
    EXPECT_GT(remap_stats.num_mmap_events, 0U);
    EXPECT_GT(remap_stats.num_sample_events_mapped, 0U);
    EXPECT_TRUE(remap_stats.did_remap);

    string remapped_perf_data2 = output_path + test_file + ".parse.remap2.out";
    ASSERT_TRUE(remap_reader.WriteFile(remapped_perf_data2));

    // No need to call CheckPerfDataAgainstBaseline again. Just compare
    // ParsedEvents.
    const auto& parser_events = parser.parsed_events();
    const auto& remap_parser_events = remap_parser.parsed_events();
    EXPECT_TRUE(std::equal(parser_events.begin(), parser_events.end(),
                           remap_parser_events.begin()));
    EXPECT_TRUE(
        ComparePerfBuildIDLists(remapped_perf_data, remapped_perf_data2));

    // This must be called when |reader| is no longer going to be used, as it
    // modifies the contents of |reader|.
    CheckFilenameAndBuildIDMethods(&reader, output_path + test_file, seed);
    ++seed;
  }
}

TEST(PerfParserTest, PipedModePerfData) {
  ScopedTempDir output_dir;
  ASSERT_FALSE(output_dir.path().empty());
  string output_path = output_dir.path();

  int seed = 0;
  for (const char* test_file : perf_test_files::GetPerfPipedDataFiles()) {
    string input_perf_data = GetTestInputFilePath(test_file);
    LOG(INFO) << "Testing " << input_perf_data;
    string output_perf_data = output_path + test_file + ".pr.out";

    PerfReader reader;
    ASSERT_TRUE(reader.ReadFile(input_perf_data));

    // Check results from the PerfReader stage.
    ASSERT_TRUE(reader.WriteFile(output_perf_data));
    EXPECT_TRUE(CheckPerfDataAgainstBaseline(output_perf_data));

    PerfParserOptions options = GetTestOptions();
    options.do_remap = true;
    PerfParser parser(&reader, options);
    ASSERT_TRUE(parser.ParseRawEvents());

    EXPECT_GT(parser.stats().num_sample_events, 0U);
    EXPECT_GT(parser.stats().num_mmap_events, 0U);
    EXPECT_GT(parser.stats().num_sample_events_mapped, 0U);
    EXPECT_TRUE(parser.stats().did_remap);

    // This must be called when |reader| is no longer going to be used, as it
    // modifies the contents of |reader|.
    CheckFilenameAndBuildIDMethods(&reader, output_path + test_file, seed);
    ++seed;
  }
}

TEST(PerfParserTest, MapsSampleEventIp) {
  std::stringstream input;

  // header
  testing::ExamplePipedPerfDataFileHeader().WriteTo(&input);

  // data

  // PERF_RECORD_HEADER_ATTR
  testing::ExamplePerfEventAttrEvent_Hardware(PERF_SAMPLE_IP |
                                              PERF_SAMPLE_TID,
                                              true /*sample_id_all*/)
      .WriteTo(&input);

  // PERF_RECORD_MMAP
  testing::ExampleMmapEvent(
      1001, 0x1c1000, 0x1000, 0, "/usr/lib/foo.so",
      testing::SampleInfo().Tid(1001)).WriteTo(&input);        // 0
  // becomes: 0x0000, 0x1000, 0
  testing::ExampleMmapEvent(
      1001, 0x1c3000, 0x2000, 0x2000, "/usr/lib/bar.so",
      testing::SampleInfo().Tid(1001)).WriteTo(&input);        // 1
  // becomes: 0x1000, 0x2000, 0

  // PERF_RECORD_MMAP2
  testing::ExampleMmap2Event(
      1002, 0x2c1000, 0x2000, 0, "/usr/lib/baz.so",
      testing::SampleInfo().Tid(1002)).WriteTo(&input);        // 2
  // becomes: 0x0000, 0x2000, 0
  testing::ExampleMmap2Event(
      1002, 0x2c3000, 0x1000, 0x3000, "/usr/lib/xyz.so",
      testing::SampleInfo().Tid(1002)).WriteTo(&input);        // 3
  // becomes: 0x1000, 0x1000, 0

  // PERF_RECORD_SAMPLE
  testing::ExamplePerfSampleEvent(
      testing::SampleInfo().Ip(0x00000000001c1000).Tid(1001))  // 4
      .WriteTo(&input);
  testing::ExamplePerfSampleEvent(
      testing::SampleInfo().Ip(0x00000000001c100a).Tid(1001))  // 5
      .WriteTo(&input);
  testing::ExamplePerfSampleEvent(
      testing::SampleInfo().Ip(0x00000000001c3fff).Tid(1001))  // 6
      .WriteTo(&input);
  testing::ExamplePerfSampleEvent(
      testing::SampleInfo().Ip(0x00000000001c2bad).Tid(1001))  // 7 (not mapped)
      .WriteTo(&input);
  testing::ExamplePerfSampleEvent(
      testing::SampleInfo().Ip(0x00000000002c100a).Tid(1002))  // 8
      .WriteTo(&input);
  testing::ExamplePerfSampleEvent(
      testing::SampleInfo().Ip(0x00000000002c5bad).Tid(1002))  // 9 (not mapped)
      .WriteTo(&input);
  testing::ExamplePerfSampleEvent(
      testing::SampleInfo().Ip(0x00000000002c300b).Tid(1002))  // 10
      .WriteTo(&input);

  // not mapped yet:
  testing::ExamplePerfSampleEvent(
      testing::SampleInfo().Ip(0x00000000002c400b).Tid(1002))  // 11
      .WriteTo(&input);
  testing::ExampleMmap2Event(
      1002, 0x2c4000, 0x1000, 0, "/usr/lib/new.so",
      testing::SampleInfo().Tid(1002)).WriteTo(&input);        // 12
  testing::ExamplePerfSampleEvent(
      testing::SampleInfo().Ip(0x00000000002c400b).Tid(1002))  // 13
      .WriteTo(&input);

  //
  // Parse input.
  //

  PerfReader reader;
  EXPECT_TRUE(reader.ReadFromString(input.str()));

  PerfParserOptions options;
  options.sample_mapping_percentage_threshold = 0;
  options.do_remap = true;
  PerfParser parser(&reader, options);
  EXPECT_TRUE(parser.ParseRawEvents());

  EXPECT_EQ(5, parser.stats().num_mmap_events);
  EXPECT_EQ(9, parser.stats().num_sample_events);
  EXPECT_EQ(6, parser.stats().num_sample_events_mapped);

  const std::vector<ParsedEvent>& events = parser.parsed_events();
  ASSERT_EQ(14, events.size());

  // MMAPs

  EXPECT_EQ(PERF_RECORD_MMAP, events[0].raw_event->header.type);
  EXPECT_EQ("/usr/lib/foo.so", string(events[0].raw_event->mmap.filename));
  EXPECT_EQ(0x0000, events[0].raw_event->mmap.start);
  EXPECT_EQ(0x1000, events[0].raw_event->mmap.len);
  EXPECT_EQ(0, events[0].raw_event->mmap.pgoff);

  EXPECT_EQ(PERF_RECORD_MMAP, events[1].raw_event->header.type);
  EXPECT_EQ("/usr/lib/bar.so", string(events[1].raw_event->mmap.filename));
  EXPECT_EQ(0x1000, events[1].raw_event->mmap.start);
  EXPECT_EQ(0x2000, events[1].raw_event->mmap.len);
  EXPECT_EQ(0x2000, events[1].raw_event->mmap.pgoff);

  EXPECT_EQ(PERF_RECORD_MMAP2, events[2].raw_event->header.type);
  EXPECT_EQ("/usr/lib/baz.so", string(events[2].raw_event->mmap2.filename));
  EXPECT_EQ(0x0000, events[2].raw_event->mmap2.start);
  EXPECT_EQ(0x2000, events[2].raw_event->mmap2.len);
  EXPECT_EQ(0, events[2].raw_event->mmap2.pgoff);

  EXPECT_EQ(PERF_RECORD_MMAP2, events[3].raw_event->header.type);
  EXPECT_EQ("/usr/lib/xyz.so", string(events[3].raw_event->mmap2.filename));
  EXPECT_EQ(0x2000, events[3].raw_event->mmap2.start);
  EXPECT_EQ(0x1000, events[3].raw_event->mmap2.len);
  EXPECT_EQ(0x3000, events[3].raw_event->mmap2.pgoff);

  // SAMPLEs

  EXPECT_EQ(PERF_RECORD_SAMPLE, events[4].raw_event->header.type);
  EXPECT_EQ("/usr/lib/foo.so", events[4].dso_and_offset.dso_name());
  EXPECT_EQ(0x0, events[4].dso_and_offset.offset());
  EXPECT_EQ(0x0, events[4].raw_event->sample.array[0]);

  EXPECT_EQ(PERF_RECORD_SAMPLE, events[5].raw_event->header.type);
  EXPECT_EQ("/usr/lib/foo.so", events[5].dso_and_offset.dso_name());
  EXPECT_EQ(0xa, events[5].dso_and_offset.offset());
  EXPECT_EQ(0xa, events[5].raw_event->sample.array[0]);

  EXPECT_EQ(PERF_RECORD_SAMPLE, events[6].raw_event->header.type);
  EXPECT_EQ("/usr/lib/bar.so", events[6].dso_and_offset.dso_name());
  EXPECT_EQ(0x2fff, events[6].dso_and_offset.offset());
  EXPECT_EQ(0x1fff, events[6].raw_event->sample.array[0]);

  EXPECT_EQ(PERF_RECORD_SAMPLE, events[7].raw_event->header.type);
  EXPECT_EQ(0x00000000001c2bad, events[7].raw_event->sample.array[0]);

  EXPECT_EQ(PERF_RECORD_SAMPLE, events[8].raw_event->header.type);
  EXPECT_EQ("/usr/lib/baz.so", events[8].dso_and_offset.dso_name());
  EXPECT_EQ(0xa, events[8].dso_and_offset.offset());
  EXPECT_EQ(0xa, events[8].raw_event->sample.array[0]);

  EXPECT_EQ(PERF_RECORD_SAMPLE, events[9].raw_event->header.type);
  EXPECT_EQ(0x00000000002c5bad, events[9].raw_event->sample.array[0]);

  EXPECT_EQ(PERF_RECORD_SAMPLE, events[10].raw_event->header.type);
  EXPECT_EQ("/usr/lib/xyz.so", events[10].dso_and_offset.dso_name());
  EXPECT_EQ(0x300b, events[10].dso_and_offset.offset());
  EXPECT_EQ(0x200b, events[10].raw_event->sample.array[0]);

  // not mapped yet:
  EXPECT_EQ(PERF_RECORD_SAMPLE, events[11].raw_event->header.type);
  EXPECT_EQ(0x00000000002c400b, events[11].raw_event->sample.array[0]);

  EXPECT_EQ(PERF_RECORD_MMAP2, events[12].raw_event->header.type);
  EXPECT_EQ("/usr/lib/new.so", string(events[12].raw_event->mmap2.filename));
  EXPECT_EQ(0x3000, events[12].raw_event->mmap2.start);
  EXPECT_EQ(0x1000, events[12].raw_event->mmap2.len);
  EXPECT_EQ(0, events[12].raw_event->mmap2.pgoff);

  EXPECT_EQ(PERF_RECORD_SAMPLE, events[13].raw_event->header.type);
  EXPECT_EQ("/usr/lib/new.so", events[13].dso_and_offset.dso_name());
  EXPECT_EQ(0xb, events[13].dso_and_offset.offset());
  EXPECT_EQ(0x300b, events[13].raw_event->sample.array[0]);
}

TEST(PerfParserTest, DsoInfoHasBuildId) {
  std::stringstream input;

  // header
  testing::ExamplePipedPerfDataFileHeader().WriteTo(&input);

  // data

  // PERF_RECORD_HEADER_ATTR
  testing::ExamplePerfEventAttrEvent_Hardware(PERF_SAMPLE_IP |
                                              PERF_SAMPLE_TID,
                                              true /*sample_id_all*/)
      .WriteTo(&input);

  // PERF_RECORD_MMAP
  testing::ExampleMmapEvent(
      1001, 0x1c1000, 0x1000, 0, "/usr/lib/foo.so",
      testing::SampleInfo().Tid(1001)).WriteTo(&input);        // 0
  // becomes: 0x0000, 0x1000, 0
  testing::ExampleMmapEvent(
      1001, 0x1c3000, 0x2000, 0x2000, "/usr/lib/bar.so",
      testing::SampleInfo().Tid(1001)).WriteTo(&input);        // 1
  // becomes: 0x1000, 0x2000, 0

  // PERF_RECORD_HEADER_BUILDID                                // N/A
  string build_id_filename("/usr/lib/foo.so\0", 2*sizeof(u64));
  ASSERT_EQ(0, build_id_filename.size() % sizeof(u64)) << "Sanity check";
  const size_t event_size =
      sizeof(struct build_id_event) +
      build_id_filename.size();
  const struct build_id_event event = {
    .header = {
      .type = PERF_RECORD_HEADER_BUILD_ID,
      .misc = 0,
      .size = static_cast<u16>(event_size),
    },
    .pid = -1,
    .build_id = {0xde, 0xad, 0xf0, 0x0d },
  };
  input.write(reinterpret_cast<const char*>(&event), sizeof(event));
  input.write(build_id_filename.data(), build_id_filename.size());

  // PERF_RECORD_SAMPLE
  testing::ExamplePerfSampleEvent(
      testing::SampleInfo().Ip(0x00000000001c1000).Tid(1001))  // 2
      .WriteTo(&input);
  testing::ExamplePerfSampleEvent(
      testing::SampleInfo().Ip(0x00000000001c300a).Tid(1001))  // 3
      .WriteTo(&input);

  //
  // Parse input.
  //

  PerfReader reader;
  EXPECT_TRUE(reader.ReadFromString(input.str()));

  PerfParserOptions options;
  options.sample_mapping_percentage_threshold = 0;
  PerfParser parser(&reader, options);
  EXPECT_TRUE(parser.ParseRawEvents());

  EXPECT_EQ(2, parser.stats().num_mmap_events);
  EXPECT_EQ(2, parser.stats().num_sample_events);
  EXPECT_EQ(2, parser.stats().num_sample_events_mapped);

  const std::vector<ParsedEvent>& events = parser.parsed_events();
  ASSERT_EQ(4, events.size());

  EXPECT_EQ("/usr/lib/foo.so", events[2].dso_and_offset.dso_name());
  EXPECT_EQ("deadf00d00000000000000000000000000000000",
            events[2].dso_and_offset.build_id());
  EXPECT_EQ("/usr/lib/bar.so", events[3].dso_and_offset.dso_name());
  EXPECT_EQ("", events[3].dso_and_offset.build_id());
}

TEST(PerfParserTest, HandlesFinishedRoundEventsAndSortsByTime) {
  // For now at least, we are ignoring PERF_RECORD_FINISHED_ROUND events.

  std::stringstream input;

  // header
  testing::ExamplePipedPerfDataFileHeader().WriteTo(&input);

  // data

  // PERF_RECORD_HEADER_ATTR
  testing::ExamplePerfEventAttrEvent_Hardware(PERF_SAMPLE_IP |
                                              PERF_SAMPLE_TID |
                                              PERF_SAMPLE_TIME,
                                              true /*sample_id_all*/)
      .WriteTo(&input);

  // PERF_RECORD_MMAP
  testing::ExampleMmapEvent(
      1001, 0x1c1000, 0x1000, 0, "/usr/lib/foo.so",
      testing::SampleInfo().Tid(1001).Time(12300010)).WriteTo(&input);
  // becomes: 0x0000, 0x1000, 0


  // PERF_RECORD_SAMPLE
  testing::ExamplePerfSampleEvent(                                // 1
      testing::SampleInfo().Ip(0x00000000001c1000).Tid(1001).Time(12300020))
      .WriteTo(&input);
  testing::ExamplePerfSampleEvent(                                // 2
      testing::SampleInfo().Ip(0x00000000001c1000).Tid(1001).Time(12300030))
      .WriteTo(&input);
  // PERF_RECORD_FINISHED_ROUND
  testing::FinishedRoundEvent().WriteTo(&input);                  // N/A

  // PERF_RECORD_SAMPLE
  testing::ExamplePerfSampleEvent(                                // 3
      testing::SampleInfo().Ip(0x00000000001c1000).Tid(1001).Time(12300050))
      .WriteTo(&input);
  testing::ExamplePerfSampleEvent(                                // 4
      testing::SampleInfo().Ip(0x00000000001c1000).Tid(1001).Time(12300040))
      .WriteTo(&input);
  // PERF_RECORD_FINISHED_ROUND
  testing::FinishedRoundEvent().WriteTo(&input);                  // N/A

  //
  // Parse input.
  //

  PerfReader reader;
  EXPECT_TRUE(reader.ReadFromString(input.str()));

  PerfParserOptions options;
  options.sample_mapping_percentage_threshold = 0;
  PerfParser parser(&reader, options);
  EXPECT_TRUE(parser.ParseRawEvents());

  EXPECT_EQ(1, parser.stats().num_mmap_events);
  EXPECT_EQ(4, parser.stats().num_sample_events);
  EXPECT_EQ(4, parser.stats().num_sample_events_mapped);

  const std::vector<ParsedEvent>& events = parser.parsed_events();
  ASSERT_EQ(5, events.size());

  struct perf_sample sample;

  EXPECT_EQ(PERF_RECORD_MMAP, events[0].raw_event->header.type);
  EXPECT_EQ(PERF_RECORD_SAMPLE, events[1].raw_event->header.type);
  reader.ReadPerfSampleInfo(*events[1].raw_event, &sample);
  EXPECT_EQ(12300020, sample.time);
  EXPECT_EQ(PERF_RECORD_SAMPLE, events[2].raw_event->header.type);
  reader.ReadPerfSampleInfo(*events[2].raw_event, &sample);
  EXPECT_EQ(12300030, sample.time);
  EXPECT_EQ(PERF_RECORD_SAMPLE, events[3].raw_event->header.type);
  reader.ReadPerfSampleInfo(*events[3].raw_event, &sample);
  EXPECT_EQ(12300050, sample.time);
  EXPECT_EQ(PERF_RECORD_SAMPLE, events[4].raw_event->header.type);
  reader.ReadPerfSampleInfo(*events[4].raw_event, &sample);
  EXPECT_EQ(12300040, sample.time);

  const std::vector<ParsedEvent*>& sorted_events =
      parser.GetEventsSortedByTime();
  ASSERT_EQ(5, sorted_events.size());

  EXPECT_EQ(PERF_RECORD_MMAP, sorted_events[0]->raw_event->header.type);
  EXPECT_EQ(PERF_RECORD_SAMPLE, sorted_events[1]->raw_event->header.type);
  reader.ReadPerfSampleInfo(*sorted_events[1]->raw_event, &sample);
  EXPECT_EQ(12300020, sample.time);
  EXPECT_EQ(PERF_RECORD_SAMPLE, sorted_events[2]->raw_event->header.type);
  reader.ReadPerfSampleInfo(*sorted_events[2]->raw_event, &sample);
  EXPECT_EQ(12300030, sample.time);
  EXPECT_EQ(PERF_RECORD_SAMPLE, sorted_events[3]->raw_event->header.type);
  reader.ReadPerfSampleInfo(*sorted_events[3]->raw_event, &sample);
  EXPECT_EQ(12300040, sample.time);
  EXPECT_EQ(PERF_RECORD_SAMPLE, sorted_events[4]->raw_event->header.type);
  reader.ReadPerfSampleInfo(*sorted_events[4]->raw_event, &sample);
  EXPECT_EQ(12300050, sample.time);
}

TEST(PerfParserTest, MmapCoversEntireAddressSpace) {
  std::stringstream input;

  // header
  testing::ExamplePipedPerfDataFileHeader().WriteTo(&input);

  // PERF_RECORD_HEADER_ATTR
  testing::ExamplePerfEventAttrEvent_Hardware(PERF_SAMPLE_IP | PERF_SAMPLE_TID,
                                              true /*sample_id_all*/)
      .WriteTo(&input);

  // PERF_RECORD_MMAP, a kernel mapping that covers the whole space.
  const uint32_t kKernelMmapPid = UINT32_MAX;
  testing::ExampleMmapEvent(
      kKernelMmapPid, 0, UINT64_MAX, 0, "[kernel.kallsyms]_text",
      testing::SampleInfo().Tid(kKernelMmapPid, 0)).WriteTo(&input);

  // PERF_RECORD_MMAP, a shared object library.
  testing::ExampleMmapEvent(
      1234, 0x7f008e000000, 0x2000000, 0, "/usr/lib/libfoo.so",
      testing::SampleInfo().Tid(1234, 1234)).WriteTo(&input);

  // PERF_RECORD_SAMPLE, within library.
  testing::ExamplePerfSampleEvent(
      testing::SampleInfo().Ip(0x7f008e123456).Tid(1234, 1235))
      .WriteTo(&input);
  // PERF_RECORD_SAMPLE, within kernel.
  testing::ExamplePerfSampleEvent(
      testing::SampleInfo().Ip(0x8000819e).Tid(1234, 1235))
      .WriteTo(&input);
  // PERF_RECORD_SAMPLE, within library.
  testing::ExamplePerfSampleEvent(
      testing::SampleInfo().Ip(0x7f008fdeadbe).Tid(1234, 1235))
      .WriteTo(&input);
  // PERF_RECORD_SAMPLE, within kernel.
  testing::ExamplePerfSampleEvent(
      testing::SampleInfo().Ip(0xffffffff8100cafe).Tid(1234, 1235))
      .WriteTo(&input);


  //
  // Parse input.
  //

  PerfReader reader;
  EXPECT_TRUE(reader.ReadFromString(input.str()));

  PerfParserOptions options;
  options.do_remap = true;
  PerfParser parser(&reader, options);
  EXPECT_TRUE(parser.ParseRawEvents());

  EXPECT_EQ(2, parser.stats().num_mmap_events);
  EXPECT_EQ(4, parser.stats().num_sample_events);
  EXPECT_EQ(4, parser.stats().num_sample_events_mapped);

  const std::vector<ParsedEvent>& events = parser.parsed_events();
  ASSERT_EQ(6, events.size());

  EXPECT_EQ(PERF_RECORD_MMAP, events[0].raw_event->header.type);
  EXPECT_EQ(string("[kernel.kallsyms]_text"),
            events[0].raw_event->mmap.filename);
  EXPECT_EQ(PERF_RECORD_MMAP, events[1].raw_event->header.type);
  EXPECT_EQ(string("/usr/lib/libfoo.so"), events[1].raw_event->mmap.filename);

  // Sample from library.
  EXPECT_EQ(PERF_RECORD_SAMPLE, events[2].raw_event->header.type);
  EXPECT_EQ("/usr/lib/libfoo.so", events[2].dso_and_offset.dso_name());
  EXPECT_EQ(0x123456, events[2].dso_and_offset.offset());

  // Sample from kernel.
  EXPECT_EQ(PERF_RECORD_SAMPLE, events[3].raw_event->header.type);
  EXPECT_EQ("[kernel.kallsyms]_text", events[3].dso_and_offset.dso_name());
  EXPECT_EQ(0x8000819e, events[3].dso_and_offset.offset());

  // Sample from library.
  EXPECT_EQ(PERF_RECORD_SAMPLE, events[4].raw_event->header.type);
  EXPECT_EQ("/usr/lib/libfoo.so", events[4].dso_and_offset.dso_name());
  EXPECT_EQ(0x1deadbe, events[4].dso_and_offset.offset());

  // Sample from kernel.
  EXPECT_EQ(PERF_RECORD_SAMPLE, events[5].raw_event->header.type);
  EXPECT_EQ("[kernel.kallsyms]_text", events[5].dso_and_offset.dso_name());
  EXPECT_EQ(0xffffffff8100cafe, events[5].dso_and_offset.offset());
}

}  // namespace quipper
