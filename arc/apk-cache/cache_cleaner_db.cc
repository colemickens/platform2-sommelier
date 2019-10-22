// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "arc/apk-cache/apk_cache_database.h"

#include <array>

#include <base/files/file_enumerator.h>
#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/logging.h>
#include <sqlite3.h>

#include "arc/apk-cache/cache_cleaner_db.h"
#include "arc/apk-cache/cache_cleaner_utils.h"

namespace apk_cache {

constexpr std::array<const char*, 4> kDatabaseFiles = {
    "index.db", "index.db-shm", "index.db-wal", "index.db-journal"};
constexpr char kFilesBase[] = "files";
constexpr char kDatabaseFile[] = "index.db";

// Value for |status| in |sessions| table.
constexpr int32_t kSessionStatusClosed = 2;

// Value for |type| in |file_entries| table.
constexpr char kFileTypeBaseApk[] = "play.apk.base";

namespace {

// Deletes all files in cache in case of database corruption.
bool DeleteCache(const base::FilePath& cache_root) {
  return RemoveUnexpectedItemsFromDir(
      cache_root,
      base::FileEnumerator::FileType::FILES |
          base::FileEnumerator::FileType::DIRECTORIES |
          base::FileEnumerator::FileType::SHOW_SYM_LINKS,
      {});
}

}  // namespace

bool CleanOpaqueFiles(const base::FilePath& cache_root) {
  if (!base::DirectoryExists(cache_root)) {
    LOG(ERROR) << "APK cache directory " << cache_root.value()
               << " does not exist";
    return false;
  }

  base::FilePath database_path = cache_root.Append(kDatabaseFile);

  if (!base::PathExists(database_path)) {
    LOG(INFO) << "Database file does not exist";
    return true;
  }

  ApkCacheDatabase db(database_path);

  int result = db.Init();
  if (result != SQLITE_OK) {
    LOG(ERROR) << "Cannot connect to database " << database_path.MaybeAsASCII();
    return false;
  }

  if (!db.CheckIntegrity()) {
    LOG(ERROR) << "Database integraty check failed";
    if (DeleteCache(cache_root)) {
      LOG(WARNING) << "Cleared cache";
      return true;
    }
    LOG(ERROR) << "Failed to clear cache";
    return false;
  }

  result = db.Close();
  if (result != SQLITE_OK) {
    LOG(ERROR) << "Failed to close database: " << result;
    return false;
  }
  return true;
}

}  // namespace apk_cache
