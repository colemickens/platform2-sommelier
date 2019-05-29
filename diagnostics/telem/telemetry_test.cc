// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <utility>

#include <base/message_loop/message_loop.h>
#include <base/files/scoped_temp_dir.h>
#include <base/files/file_util.h>
#include <base/time/time.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "diagnostics/common/file_test_utils.h"
#include "diagnostics/telem/telem_parsers.h"
#include "diagnostics/telem/telemetry.h"
#include "diagnostics/telem/telemetry_item_enum.h"

namespace diagnostics {

constexpr base::TimeDelta kImpossiblyLongTimeDelta =
    base::TimeDelta::FromSeconds(86400 * 365);

// Test that retrieving the memory group works end-to-end.
TEST(TelemetryIntegration, TestCacheForMemoryGroup) {
  base::ScopedTempDir fake_root;
  CHECK(fake_root.CreateUniqueTempDir());

  const base::FilePath root_dir = fake_root.GetPath();
  base::FilePath meminfo = root_dir.Append("proc/meminfo");
  std::string fake_meminfo_file_contents =
      "MemTotal:      3906320 kB\nMemFree:      873180 kB\n";
  WriteFileAndCreateParentDirs(meminfo, fake_meminfo_file_contents);

  Telemetry telemetry(root_dir);
  auto values = telemetry.GetGroup(TelemetryGroupEnum::kMemory,
                                   base::TimeDelta::FromSeconds(0));
  ASSERT_EQ(values.size(), 2);
  for (auto& pair : values) {
    DCHECK(pair.second.has_value());
  }

  constexpr int kFakeMemTotalMebibytes = 3906320 / 1024;
  // Make sure the cache is still valid. Delete the file that stores the info to
  // be sure.
  CHECK(fake_root.Delete());
  EXPECT_EQ(telemetry
                .GetItem(TelemetryItemEnum::kMemTotalMebibytes,
                         kImpossiblyLongTimeDelta)
                .value(),
            base::Value(kFakeMemTotalMebibytes));
}

// Test that retrieving the Memory group fails gracefully when a procfs file is
// missing.
TEST(TelemetryIntegration, TestNonexistentFileForMemoryGroup) {
  base::ScopedTempDir fake_root;
  CHECK(fake_root.CreateUniqueTempDir());

  const base::FilePath root_dir = fake_root.GetPath();
  base::FilePath procfs = root_dir.Append("proc");
  base::CreateDirectory(procfs);

  Telemetry telemetry(root_dir);
  auto values = telemetry.GetGroup(TelemetryGroupEnum::kMemory,
                                   base::TimeDelta::FromSeconds(0));
  // Even when the file doesn't exist, we should return a null value for the
  // items.
  ASSERT_EQ(values.size(), 2);
  for (auto& pair : values) {
    DCHECK(!pair.second.has_value());
  }
}

// Test that retrieving the Memory group fails gracefully when the meminfo file
// is empty. Essentially just testing an edge case.
TEST(TelemetryIntegration, TestEmptyFileForMemoryGroup) {
  base::ScopedTempDir fake_root;
  CHECK(fake_root.CreateUniqueTempDir());

  const base::FilePath root_dir = fake_root.GetPath();
  base::FilePath meminfo = root_dir.Append("proc/meminfo");
  WriteFileAndCreateParentDirs(meminfo, "");

  Telemetry telemetry(root_dir);
  auto values = telemetry.GetGroup(TelemetryGroupEnum::kMemory,
                                   base::TimeDelta::FromSeconds(0));
  // Even when the file is empty, we should return a null value for the items.
  ASSERT_EQ(values.size(), 2);
  for (auto& pair : values) {
    DCHECK(!pair.second.has_value());
  }
}

}  // namespace diagnostics
