// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>

#include <base/logging.h>
#include <base/test/simple_test_tick_clock.h>
#include <base/time/time.h>
#include <base/values.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "diagnostics/telem/telem_cache.h"
#include "diagnostics/telem/telemetry_item_enum.h"

using testing::StrictMock;

namespace diagnostics {

namespace {

// String values that telemetry items can take.
constexpr char kFirstStrValue[] = "test_val1";
constexpr char kSecondStrValue[] = "test_val2";

// Longest TimeDelta that an item inserted into the cache is valid for.
constexpr base::TimeDelta kInsertionDelta = base::TimeDelta::FromSeconds(2);
// Longer TimeDelta than the insertion time. Requesting an item with this delta
// should pass the cache's timeout check.
constexpr base::TimeDelta kAfterInsertionDelta =
    base::TimeDelta::FromSeconds(3);
// Shorter TimeDelta than the insertion time. Requesting an item with this delta
// should fail the cache's timeout check.
constexpr base::TimeDelta kBeforeInsertionDelta =
    base::TimeDelta::FromSeconds(1);

}  // namespace

class TelemCacheTest : public ::testing::Test {
 public:
  TelemCacheTest() = default;

  TelemCache* cache() { return cache_.get(); }

  base::SimpleTestTickClock* clock() { return &clock_; }

 private:
  base::SimpleTestTickClock clock_;
  std::unique_ptr<TelemCache> cache_ = std::make_unique<TelemCache>(&clock_);

  DISALLOW_COPY_AND_ASSIGN(TelemCacheTest);
};

// Test that an empty cache does not have a valid entry.
TEST_F(TelemCacheTest, EmptyCacheInvalidEntry) {
  EXPECT_FALSE(cache()->IsValid(TelemetryItemEnum::kMemTotalMebibytes,
                                kAfterInsertionDelta));
}

// Test that we get an appropriate error if we attempt to access an
// entry which does not exist.
TEST_F(TelemCacheTest, AccessMissingEntry) {
  EXPECT_EQ(cache()->GetParsedData(TelemetryItemEnum::kMemTotalMebibytes),
            nullptr);
}

// Test that we can insert and retrieve an item with the cache.
TEST_F(TelemCacheTest, InsertNewEntry) {
  cache()->SetParsedData(TelemetryItemEnum::kMemTotalMebibytes,
                         std::make_unique<base::Value>(kFirstStrValue));
  std::string string_val;
  const base::Value* returned_data =
      cache()->GetParsedData(TelemetryItemEnum::kMemTotalMebibytes);
  ASSERT_TRUE(returned_data != nullptr);
  EXPECT_TRUE(returned_data->GetAsString(&string_val));
  EXPECT_EQ(string_val, kFirstStrValue);
}

// Test that an old cached value is not valid.
TEST_F(TelemCacheTest, InvalidOldEntry) {
  cache()->SetParsedData(TelemetryItemEnum::kMemTotalMebibytes,
                         std::make_unique<base::Value>(kFirstStrValue));
  clock()->Advance(kInsertionDelta);
  EXPECT_FALSE(cache()->IsValid(TelemetryItemEnum::kMemTotalMebibytes,
                                kBeforeInsertionDelta));
}

// Test that a recent cached value is valid.
TEST_F(TelemCacheTest, ValidRecentEntry) {
  cache()->SetParsedData(TelemetryItemEnum::kMemTotalMebibytes,
                         std::make_unique<base::Value>(kFirstStrValue));
  clock()->Advance(kInsertionDelta);
  EXPECT_TRUE(cache()->IsValid(TelemetryItemEnum::kMemTotalMebibytes,
                               kAfterInsertionDelta));
}

// Test that we can update a cached item's value.
TEST_F(TelemCacheTest, UpdateExistingEntry) {
  cache()->SetParsedData(TelemetryItemEnum::kMemTotalMebibytes,
                         std::make_unique<base::Value>(kFirstStrValue));
  std::string string_val;
  const base::Value* returned_data =
      cache()->GetParsedData(TelemetryItemEnum::kMemTotalMebibytes);
  ASSERT_TRUE(returned_data != nullptr);
  EXPECT_TRUE(returned_data->GetAsString(&string_val));
  EXPECT_EQ(string_val, kFirstStrValue);
  cache()->SetParsedData(TelemetryItemEnum::kMemTotalMebibytes,
                         std::make_unique<base::Value>(kSecondStrValue));
  returned_data = cache()->GetParsedData(TelemetryItemEnum::kMemTotalMebibytes);
  ASSERT_TRUE(returned_data != nullptr);
  EXPECT_TRUE(returned_data->GetAsString(&string_val));
  EXPECT_EQ(string_val, kSecondStrValue);
}

// Test that we can invalidate the cache.
TEST_F(TelemCacheTest, InvalidateExistingEntries) {
  cache()->SetParsedData(TelemetryItemEnum::kMemTotalMebibytes,
                         std::make_unique<base::Value>(kFirstStrValue));
  cache()->SetParsedData(TelemetryItemEnum::kStat,
                         std::make_unique<base::Value>(kSecondStrValue));
  clock()->Advance(kInsertionDelta);
  EXPECT_TRUE(cache()->IsValid(TelemetryItemEnum::kMemTotalMebibytes,
                               kAfterInsertionDelta));
  EXPECT_TRUE(cache()->IsValid(TelemetryItemEnum::kStat, kAfterInsertionDelta));
  cache()->Invalidate();
  EXPECT_FALSE(cache()->IsValid(TelemetryItemEnum::kMemTotalMebibytes,
                                kAfterInsertionDelta));
  EXPECT_FALSE(
      cache()->IsValid(TelemetryItemEnum::kStat, kAfterInsertionDelta));
}

}  // namespace diagnostics
