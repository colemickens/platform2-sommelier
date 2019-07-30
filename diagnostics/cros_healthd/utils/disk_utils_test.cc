// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <iterator>
#include <string>

#include <base/files/file_path.h>
#include <base/files/scoped_temp_dir.h>
#include <gtest/gtest.h>

#include "diagnostics/common/file_test_utils.h"
#include "diagnostics/cros_healthd/utils/disk_utils.h"

namespace diagnostics {
namespace disk_utils {
namespace {

const char kRelativeSKUNumberPath[] = "sys/firmware/vpd/ro/sku_number";
const char kFakeSKUNumber[] = "ABCD&^A";

// Test that we can read the cached VPD info, when it exists.
TEST(DiskUtils, TestFetchCachedVpdInfo) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath root_dir = temp_dir.GetPath();
  EXPECT_TRUE(WriteFileAndCreateParentDirs(
      root_dir.Append(kRelativeSKUNumberPath), kFakeSKUNumber));
  auto vpd_info = FetchCachedVpdInfo(root_dir);
  EXPECT_EQ(vpd_info->sku_number, kFakeSKUNumber);
}

// Test that reading cached VPD info that does not exist fails gracefully.
TEST(DiskUtils, TestFetchCachedVpdInfoNoFile) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  auto vpd_info = FetchCachedVpdInfo(temp_dir.GetPath());
  EXPECT_EQ(vpd_info->sku_number, "");
}

}  // namespace
}  // namespace disk_utils
}  // namespace diagnostics
