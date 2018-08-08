// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crash-reporter/util.h"

#include <base/files/file_util.h>
#include <base/files/scoped_temp_dir.h>
#include <gtest/gtest.h>

#include "crash-reporter/crash_sender_paths.h"
#include "crash-reporter/paths.h"
#include "crash-reporter/test_util.h"

namespace util {

class CrashCommonUtilTest : public testing::Test {
 private:
  void SetUp() override {
    ASSERT_TRUE(scoped_temp_dir_.CreateUniqueTempDir());
    test_dir_ = scoped_temp_dir_.GetPath();
    paths::SetPrefixForTesting(test_dir_);
  }

  void TearDown() override { paths::SetPrefixForTesting(base::FilePath()); }

  base::FilePath test_dir_;
  base::ScopedTempDir scoped_temp_dir_;
};

TEST_F(CrashCommonUtilTest, IsCrashTestInProgress) {
  EXPECT_FALSE(IsCrashTestInProgress());
  ASSERT_TRUE(
      test_util::CreateFile(paths::GetAt(paths::kSystemRunStateDirectory,
                                         paths::kCrashTestInProgress),
                            ""));
  EXPECT_TRUE(IsCrashTestInProgress());
}

TEST_F(CrashCommonUtilTest, IsDeveloperImage) {
  EXPECT_FALSE(IsDeveloperImage());

  ASSERT_TRUE(test_util::CreateFile(paths::Get(paths::kLeaveCoreFile), ""));
  EXPECT_TRUE(IsDeveloperImage());

  ASSERT_TRUE(
      test_util::CreateFile(paths::GetAt(paths::kSystemRunStateDirectory,
                                         paths::kCrashTestInProgress),
                            ""));
  EXPECT_FALSE(IsDeveloperImage());
}

}  // namespace util
