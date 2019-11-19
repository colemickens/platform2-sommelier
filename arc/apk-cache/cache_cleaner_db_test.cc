// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/files/scoped_temp_dir.h>
#include <base/time/time.h>
#include <gtest/gtest.h>
#include <sqlite3.h>

#include "arc/apk-cache/apk_cache_database.h"
#include "arc/apk-cache/apk_cache_database_test_utils.h"
#include "arc/apk-cache/cache_cleaner_db.h"

namespace apk_cache {

namespace {

constexpr char kBrokenDatabaseContent[] = "test broken db file content";
constexpr char kTestSessionSource[] = "unit_test_session_source";
constexpr int64_t kTestSessionId = 123;

constexpr int64_t kTestBaseApkId = 1234;
constexpr int64_t kTestAttachmentId = 1235;
constexpr char kTestPackageName[] = "com.package.test";
constexpr int64_t kTestVersionCode = 1234;
constexpr char kTestAttachmentType[] = "test.foo.bar";
constexpr char kTestFileContent[] = "test file content";
constexpr char kTestFileHash[] = "2Q7xZR_Z51Y-GhRQoWvXhOmn4tPfD1p5jfwb33CmSuo";
constexpr int32_t kTestPackagePriority = 100;

bool CreateSession(const base::FilePath& db_path, int64_t id, int32_t status) {
  Session session;
  session.id = id;
  session.source = kTestSessionSource;
  session.timestamp = base::Time::Now();
  session.status = status;
  return InsertSessionForTesting(db_path, session);
}

bool CreateFileEntry(const base::FilePath& db_path,
                     const base::FilePath& files_path,
                     int64_t id,
                     const std::string& package_name,
                     int64_t version_code,
                     const std::string& type) {
  FileEntry file_entry;
  file_entry.id = id;
  file_entry.package_name = package_name;
  file_entry.version_code = version_code;
  file_entry.type = type;
  file_entry.size = strlen(kTestFileContent);
  file_entry.hash = std::string(kTestFileHash);
  file_entry.access_time = base::Time::Now();
  file_entry.priority = kTestPackagePriority;
  file_entry.session_id = kTestSessionId;
  if (!InsertFileEntryForTesting(db_path, file_entry))
    return false;

  base::FilePath file_path = files_path.Append(GetFileNameById(id));
  return base::WriteFile(file_path, kTestFileContent, strlen(kTestFileContent));
}

bool CreateValidPackage(const base::FilePath& db_path,
                        const base::FilePath& files_path) {
  return CreateSession(db_path, kTestSessionId, kSessionStatusClosed) &&
         CreateFileEntry(db_path, files_path, kTestBaseApkId, kTestPackageName,
                         kTestVersionCode, kFileTypeBaseApk) &&
         CreateFileEntry(db_path, files_path, kTestAttachmentId,
                         kTestPackageName, kTestVersionCode,
                         kTestAttachmentType);
}

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

// If database does not exist, files directory should be removed.
TEST_F(CacheCleanerDBTest, DatabaseNotExist) {
  // Create files directory.
  base::FilePath files_path = temp_path().Append(kFilesBase);
  ASSERT_TRUE(base::CreateDirectory(files_path));
  // Write a random file to files directory.
  base::FilePath file_path = files_path.Append("test");
  ASSERT_TRUE(
      base::WriteFile(file_path, kTestFileContent, strlen(kTestFileContent)));
  // Clean.
  EXPECT_TRUE(OpaqueFilesCleaner(temp_path()).Clean());
  // Files directory should be removed.
  EXPECT_FALSE(base::PathExists(files_path));
}

// If database is empty, files directory should be removed.
TEST_F(CacheCleanerDBTest, EmptyDatabase) {
  // Write empty database file.
  base::FilePath db_path = temp_path().Append(kDatabaseFile);
  ASSERT_TRUE(base::WriteFile(db_path, "\0", 1));
  ASSERT_TRUE(base::PathExists(db_path));
  // Create files directory.
  base::FilePath files_path = temp_path().Append(kFilesBase);
  ASSERT_TRUE(base::CreateDirectory(files_path));
  // Write a random file to files directory.
  base::FilePath file_path = files_path.Append("test");
  ASSERT_TRUE(
      base::WriteFile(file_path, kTestFileContent, strlen(kTestFileContent)));
  // Clean.
  EXPECT_TRUE(OpaqueFilesCleaner(temp_path()).Clean());
  // Files directory should be removed.
  EXPECT_FALSE(base::PathExists(files_path));
}

// Correct database structure should pass integrity test.
TEST_F(CacheCleanerDBTest, ApkCacheDatabase) {
  base::FilePath db_path = temp_path().Append(kDatabaseFile);
  ASSERT_EQ(CreateDatabaseForTesting(db_path), SQLITE_OK);
  EXPECT_TRUE(base::PathExists(db_path));
  EXPECT_TRUE(OpaqueFilesCleaner(temp_path()).Clean());
}

// If database file is broken, it should be removed with files directory.
TEST_F(CacheCleanerDBTest, BrokenDatabaseFile) {
  // Write random content to database file.
  base::FilePath db_path = temp_path().Append(kDatabaseFile);
  base::WriteFile(db_path, kBrokenDatabaseContent,
                  strlen(kBrokenDatabaseContent));
  EXPECT_TRUE(base::PathExists(db_path));
  // Create files directory.
  base::FilePath files_path = temp_path().Append(kFilesBase);
  ASSERT_TRUE(base::CreateDirectory(files_path));
  // Write a random file to files directory.
  base::FilePath file_path = files_path.Append("test");
  ASSERT_TRUE(
      base::WriteFile(file_path, kTestFileContent, strlen(kTestFileContent)));
  // Clean.
  EXPECT_TRUE(OpaqueFilesCleaner(temp_path()).Clean());
  // Both database file and files directory should be removed.
  EXPECT_FALSE(base::PathExists(db_path));
  EXPECT_FALSE(base::PathExists(files_path));
}

// Cache cleaner should create a session before cleaning.
TEST_F(CacheCleanerDBTest, CacheCleanerSession) {
  base::FilePath db_path = temp_path().Append(kDatabaseFile);
  ASSERT_EQ(CreateDatabaseForTesting(db_path), SQLITE_OK);
  EXPECT_TRUE(base::PathExists(db_path));
  // Clean.
  OpaqueFilesCleaner cleaner(temp_path());
  EXPECT_TRUE(cleaner.Clean());
  // Cache cleaner session should be created.
  ApkCacheDatabase db(db_path);
  ASSERT_EQ(db.Init(), SQLITE_OK);
  auto sessions = db.GetSessions();
  ASSERT_TRUE(sessions);
  ASSERT_EQ(sessions->size(), 1);
  ASSERT_EQ((*sessions)[0].source, kCacheCleanerSessionSource);
}

// If an open session is not expired, cleaner should exit.
TEST_F(CacheCleanerDBTest, OtherSessionActive) {
  base::FilePath db_path = temp_path().Append(kDatabaseFile);
  ASSERT_EQ(CreateDatabaseForTesting(db_path), SQLITE_OK);
  EXPECT_TRUE(base::PathExists(db_path));
  // Create an open session.
  ASSERT_TRUE(CreateSession(db_path, kTestSessionId, kSessionStatusOpen));
  // Clean.
  OpaqueFilesCleaner cleaner(temp_path());
  EXPECT_TRUE(cleaner.Clean());
  // Cache cleaner session should not be created.
  ApkCacheDatabase db(db_path);
  ASSERT_EQ(db.Init(), SQLITE_OK);
  auto sessions = db.GetSessions();
  ASSERT_TRUE(sessions);
  for (Session session : *sessions)
    EXPECT_NE(session.source, kCacheCleanerSessionSource);
}

// Expired open sessions should be removed.
TEST_F(CacheCleanerDBTest, ExpiredOpenSessions) {
  base::FilePath db_path = temp_path().Append(kDatabaseFile);
  ASSERT_EQ(CreateDatabaseForTesting(db_path), SQLITE_OK);
  EXPECT_TRUE(base::PathExists(db_path));
  // Create valid package.
  base::FilePath files_path = temp_path().Append(kFilesBase);
  ASSERT_TRUE(base::CreateDirectory(files_path));
  EXPECT_TRUE(CreateValidPackage(db_path, files_path));
  // Change session status to open.
  UpdateSessionStatusForTesting(db_path, kTestSessionId, kSessionStatusOpen);
  // Let session expire.
  UpdateSessionTimestampForTesting(
      db_path, kTestSessionId,
      base::Time::Now() - kSessionMaxAge - base::TimeDelta::FromSeconds(1));
  // Clean.
  EXPECT_TRUE(OpaqueFilesCleaner(temp_path()).Clean());
  // Test session should be removed.
  ApkCacheDatabase db(db_path);
  ASSERT_EQ(db.Init(), SQLITE_OK);
  auto sessions = db.GetSessions();
  ASSERT_TRUE(sessions);
  bool session_exists = false;
  for (Session session : *sessions)
    if (session.id == kTestSessionId) {
      session_exists = true;
      break;
    }
  EXPECT_FALSE(session_exists);
  // Package should be removed.
  auto file_entries = db.GetFileEntries();
  ASSERT_TRUE(file_entries);
  EXPECT_EQ(file_entries->size(), 0);
}

}  // namespace apk_cache
