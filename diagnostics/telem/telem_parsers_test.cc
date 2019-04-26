// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <utility>

#include <base/message_loop/message_loop.h>
#include <base/time/time.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "diagnostics/telem/telem_parsers.h"
#include "diagnostics/telem/telemetry.h"
#include "diagnostics/telem/telemetry_item_enum.h"

using ::testing::_;

namespace diagnostics {

char const* kFakePerCPUIdleTimes[] = {"653435243543", "235435413"};
constexpr char kFakeBadLoadavgFileContents[] = "0.82 0.61 0.52 2 370 30707\n";
constexpr char kFakeBadMeminfoFileContents[] =
    "MemTotal:      3906320\nMemfree:      873180 kB\n";
constexpr char kFakeBadStatFileContents[] =
    "cpu  0 0 165432156432413546\npu0 0 0 0 653435243543\ncpu1 0 0 0 "
    "235435413\nctxt 5345634354";
constexpr char kFakeLoadavgFileContents[] = "0.82 0.61 0.52 2/370 30707\n";
constexpr char kFakeMeminfoFileContents[] =
    "MemTotal:      3906320 kB\nMemFree:      873180 kB\n";
constexpr char kFakeTotalCPUIdleTime[] = "165432156432413546";
constexpr int kFakeMemFreeMebibytes = 852;
constexpr int kFakeMemTotalMebibytes = 3906320 / 1024;
constexpr int kFakeNumExistingEntities = 370;
constexpr int kFakeNumRunnableEntities = 2;
constexpr char kFakeStatFileContents[] =
    "cpu  0 0 0 165432156432413546\ncpu0 0 0 0 653435243543\ncpu1 0 0 0 "
    "235435413\nctxt 5345634354";

struct CacheWriterImpl : public CacheWriter {
  ~CacheWriterImpl() {}

  void SetParsedData(TelemetryItemEnum item,
                     base::Optional<base::Value> data) override {
    cache_[item] = data;
  }

  void CheckParsedDataFor(TelemetryItemEnum item, base::Value data) {
    ASSERT_EQ(cache_.at(item).value(), data);
  }

  void CheckParsedDataIsNull(TelemetryItemEnum item) {
    CHECK(!cache_.at(item).has_value());
  }

  std::map<TelemetryItemEnum, base::Optional<base::Value>> cache_;
};

// Test that we can retrieve kMemTotalMebibytes and kMemFreeMebibytes.
TEST(TelemParsers, GetMemTotal) {
  FileDump file_dump{"", "", kFakeMeminfoFileContents};

  CacheWriterImpl test_cache;
  ParseDataFromProcMeminfo({file_dump}, &test_cache);

  test_cache.CheckParsedDataFor(TelemetryItemEnum::kMemTotalMebibytes,
                                base::Value(kFakeMemTotalMebibytes));
  test_cache.CheckParsedDataFor(TelemetryItemEnum::kMemFreeMebibytes,
                                base::Value(kFakeMemFreeMebibytes));
}

// Test that an incorrectly formatted /proc/meminfo will fail to parse.
TEST(TelemParsers, GetBadMeminfo) {
  FileDump file_dump{"", "", kFakeBadMeminfoFileContents};

  CacheWriterImpl test_cache;
  ParseDataFromProcMeminfo({file_dump}, &test_cache);

  test_cache.CheckParsedDataIsNull(TelemetryItemEnum::kMemTotalMebibytes);
  test_cache.CheckParsedDataIsNull(TelemetryItemEnum::kMemFreeMebibytes);
}

// Test that we can retrieve kNumRunnableEntities and kNumExistingEntities.
TEST(TelemParsers, GetRunnableEntities) {
  FileDump file_dump{"", "", kFakeLoadavgFileContents};

  CacheWriterImpl test_cache;
  ParseDataFromProcLoadavg({file_dump}, &test_cache);

  test_cache.CheckParsedDataFor(TelemetryItemEnum::kNumRunnableEntities,
                                base::Value(kFakeNumRunnableEntities));
  test_cache.CheckParsedDataFor(TelemetryItemEnum::kNumExistingEntities,
                                base::Value(kFakeNumExistingEntities));
}

// Test that an incorrectly formatted /proc/loadavg will fail to parse.
TEST(TelemParsers, GetBadLoadAvg) {
  FileDump file_dump{"", "", kFakeBadLoadavgFileContents};

  CacheWriterImpl test_cache;
  ParseDataFromProcLoadavg({file_dump}, &test_cache);

  test_cache.CheckParsedDataIsNull(TelemetryItemEnum::kNumRunnableEntities);
  test_cache.CheckParsedDataIsNull(TelemetryItemEnum::kNumExistingEntities);
}

// Test that we can retrieve kTotalIdleTimeUserHz and kIdleTimePerCPUUserHz.
TEST(TelemParsers, GetCPUTimes) {
  FileDump file_dump{"", "", kFakeStatFileContents};

  CacheWriterImpl test_cache;
  ParseDataFromProcStat({file_dump}, &test_cache);

  test_cache.CheckParsedDataFor(TelemetryItemEnum::kTotalIdleTimeUserHz,
                                base::Value(kFakeTotalCPUIdleTime));

  base::ListValue fake_idle_times;
  for (auto fakePerCPUIdleTime : kFakePerCPUIdleTimes) {
    fake_idle_times.AppendString(fakePerCPUIdleTime);
  }

  test_cache.CheckParsedDataFor(TelemetryItemEnum::kIdleTimePerCPUUserHz,
                                fake_idle_times);
}

// Test that an incorrectly formatted /proc/stat will fail gracefully.
TEST(TelemParsers, GetEmptyStat) {
  FileDump file_dump{"", "", ""};

  CacheWriterImpl test_cache;
  ParseDataFromProcStat({file_dump}, &test_cache);

  test_cache.CheckParsedDataIsNull(TelemetryItemEnum::kIdleTimePerCPUUserHz);
}

// Test that an empty /proc/stat will fail gracefully.
TEST(TelemParsers, GetBadStat) {
  FileDump file_dump{"", "", kFakeBadStatFileContents};

  CacheWriterImpl test_cache;
  ParseDataFromProcStat({file_dump}, &test_cache);

  test_cache.CheckParsedDataIsNull(TelemetryItemEnum::kIdleTimePerCPUUserHz);
}

// Test that an incorrectly formatted /proc/stat will fail to parse.
TEST(TelemParsers, GetNoFileStat) {
  CacheWriterImpl test_cache;
  ParseDataFromProcStat({}, &test_cache);

  test_cache.CheckParsedDataIsNull(TelemetryItemEnum::kIdleTimePerCPUUserHz);
}

}  // namespace diagnostics
