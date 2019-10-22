// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/files/scoped_temp_dir.h>
#include <gtest/gtest.h>
#include <sqlite3.h>

#include "arc/apk-cache/apk_cache_database.h"
#include "arc/apk-cache/apk_cache_database_test_utils.h"
#include "arc/apk-cache/cache_cleaner_db.h"

namespace apk_cache {

namespace {

constexpr char kBrokenDatabaseContent[] = "test broken db file content";

}  // namespace

class CacheCleanerDBTest : public testing::Test {
 public:
  const base::FilePath& temp_path() const { return temp_dir_.GetPath(); }

 protected:
  CacheCleanerDBTest() = default;

  // Not copyable or movable.
  CacheCleanerDBTest(const CacheCleanerDBTest&) = delete;
  CacheCleanerDBTest& operator=(const CacheCleanerDBTest&) = delete;

  void SetUp() override { ASSERT_TRUE(temp_dir_.CreateUniqueTempDir()); }
  void TearDown() override { ASSERT_TRUE(temp_dir_.Delete()); }

 private:
  base::ScopedTempDir temp_dir_;
};

// Empty database should pass integrity test.
TEST_F(CacheCleanerDBTest, EmptyDatabase) {
  EXPECT_TRUE(CleanOpaqueFiles(temp_path()));
}

// Correct database structure should pass integrity test.
TEST_F(CacheCleanerDBTest, ApkCacheDatabase) {
  base::FilePath db_path = temp_path().Append(kDatabaseFile);
  ASSERT_EQ(CreateDatabase(db_path), SQLITE_OK);
  EXPECT_TRUE(base::PathExists(db_path));
  EXPECT_TRUE(CleanOpaqueFiles(temp_path()));
}

// Broken database file should fail integrity test and be removed.
TEST_F(CacheCleanerDBTest, BrokenDatabaseFile) {
  base::FilePath db_path = temp_path().Append(kDatabaseFile);
  base::WriteFile(db_path, kBrokenDatabaseContent,
                  strlen(kBrokenDatabaseContent));
  EXPECT_TRUE(base::PathExists(db_path));
  EXPECT_TRUE(CleanOpaqueFiles(temp_path()));
  EXPECT_FALSE(base::PathExists(db_path));
}

}  // namespace apk_cache
