// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ARC_APK_CACHE_APK_CACHE_DATABASE_TEST_UTILS_H_
#define ARC_APK_CACHE_APK_CACHE_DATABASE_TEST_UTILS_H_

#include <stdint.h>

namespace base {
class FilePath;
class Time;
}  // namespace base

namespace apk_cache {
struct Session;
struct FileEntry;
}  // namespace apk_cache

namespace apk_cache {

// Create database and tables for testing.
int CreateDatabaseForTesting(const base::FilePath& db_path);
// Insert session into database for testing.
bool InsertSessionForTesting(const base::FilePath& db_path,
                             const Session& session);
// Insert file entry into database for testing.
bool InsertFileEntryForTesting(const base::FilePath& db_path,
                               const FileEntry& file_entry);
// Update session timestamp for testing.
bool UpdateSessionTimestampForTesting(const base::FilePath& db_path,
                                      int64_t id,
                                      const base::Time& timestamp);
// Update session status for testing.
bool UpdateSessionStatusForTesting(const base::FilePath& db_path,
                                   int64_t id,
                                   int32_t status);

}  // namespace apk_cache

#endif  // ARC_APK_CACHE_APK_CACHE_DATABASE_TEST_UTILS_H_
