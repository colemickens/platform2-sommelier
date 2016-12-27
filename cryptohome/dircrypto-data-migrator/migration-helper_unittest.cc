// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cryptohome/dircrypto-data-migrator/migration-helper.h"

#include <gtest/gtest.h>

#include <base/files/file_path.h>
#include <base/files/scoped_temp_dir.h>

#include "cryptohome/mock_platform.h"

using base::FilePath;
using base::ScopedTempDir;

namespace cryptohome {
namespace dircrypto_data_migrator {

class MigrationHelperTest : public ::testing::Test {
 public:
  virtual ~MigrationHelperTest() {}

  void SetUp() override {
    ASSERT_TRUE(status_files_dir_.CreateUniqueTempDir());
  }

  void TearDown() override { EXPECT_TRUE(status_files_dir_.Delete()); }

 protected:
  ScopedTempDir status_files_dir_;
};

TEST_F(MigrationHelperTest, EmptyTest) {
  MockPlatform platform;
  EXPECT_TRUE(platform.DirectoryExists(status_files_dir_.path()));
  MigrationHelper helper(&platform, status_files_dir_.path());
}

}  // namespace dircrypto_data_migrator
}  // namespace cryptohome
