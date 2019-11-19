// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "arc/apk-cache/cache_cleaner_db.h"

#include <array>
#include <inttypes.h>
#include <iomanip>
#include <stdint.h>

#include <base/files/file_enumerator.h>
#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/logging.h>
#include <base/strings/stringprintf.h>
#include <base/time/time.h>
#include <sqlite3.h>

#include "arc/apk-cache/apk_cache_database.h"
#include "arc/apk-cache/cache_cleaner_utils.h"

namespace apk_cache {

constexpr std::array<const char*, 4> kDatabaseFiles = {
    "index.db", "index.db-shm", "index.db-wal", "index.db-journal"};
constexpr char kFilesBase[] = "files";
constexpr char kDatabaseFile[] = "index.db";

// Value for |status| in |sessions| table.
constexpr int32_t kSessionStatusOpen = 1;
constexpr int32_t kSessionStatusClosed = 2;

// Value for |type| in |file_entries| table.
constexpr char kFileTypeBaseApk[] = "play.apk.base";

// Cache cleaner session source.
constexpr char kCacheCleanerSessionSource[] = "cache_cleaner";

// Maximum age of sessions.
constexpr base::TimeDelta kSessionMaxAge = base::TimeDelta::FromMinutes(10);

// Maximum age of cached files. If a file expires, the whole package will be
// removed.
constexpr base::TimeDelta kValidityPeriod = base::TimeDelta::FromDays(30);

std::string GetFileNameById(int64_t id) {
  return base::StringPrintf("%016" PRIx64, id);
}

OpaqueFilesCleaner::OpaqueFilesCleaner(const base::FilePath& cache_root)
    : cache_root_(cache_root),
      db_path_(cache_root.Append(kDatabaseFile)),
      files_path_(cache_root.Append(kFilesBase)) {}

OpaqueFilesCleaner::~OpaqueFilesCleaner() = default;

bool OpaqueFilesCleaner::Clean() {
  if (!base::DirectoryExists(cache_root_)) {
    LOG(ERROR) << "APK cache directory " << cache_root_.value()
               << " does not exist";
    return false;
  }

  // Delete files directory if database file does not exist.
  if (!base::PathExists(db_path_)) {
    LOG(INFO) << "Database file does not exist";
    return DeleteFiles();
  }

  ApkCacheDatabase db(db_path_);

  if (db.Init() != SQLITE_OK) {
    LOG(ERROR) << "Cannot connect to database " << db_path_.MaybeAsASCII();
    return DeleteCache();
  }

  // Delete the whole cache if database fails integrity check.
  if (!db.CheckIntegrity()) {
    LOG(ERROR) << "Database integrity check failed";
    return DeleteCache();
  }

  // Delete files directory if database is an empty file, i.e. desired tables
  // do not exist.
  if (!db.SessionsTableExists()) {
    LOG(INFO) << "Database is empty";
    return DeleteFiles();
  }

  // Clean stale sessions
  if (!CleanStaleSessions(db)) {
    LOG(ERROR) << "Failed to clean stale sessions";
    DeleteCache();
    return false;
  }

  // Exit normally if any other session is active.
  if (IsOtherSessionActive(db))
    return true;

  // Open cache cleaner session.
  int64_t session_id = OpenSession(db);
  if (session_id == 0) {
    LOG(ERROR) << "Failed to create session";
    DeleteCache();
    return false;
  }

  // Close cache cleaner session.
  bool success = CloseSession(db, session_id);

  int result = db.Close();
  if (result != SQLITE_OK) {
    LOG(ERROR) << "Failed to close database: " << result;
    return false;
  }
  return success;
}

bool OpaqueFilesCleaner::DeleteCache() const {
  if (RemoveUnexpectedItemsFromDir(
          cache_root_,
          base::FileEnumerator::FileType::FILES |
              base::FileEnumerator::FileType::DIRECTORIES |
              base::FileEnumerator::FileType::SHOW_SYM_LINKS,
          {})) {
    LOG(INFO) << "Cleared cache";
    return true;
  }

  LOG(ERROR) << "Failed to delete cache";
  return false;
}

bool OpaqueFilesCleaner::DeleteFiles() const {
  if (base::PathExists(files_path_) && base::DeleteFile(files_path_, true))
    return true;

  LOG(ERROR) << "Failed to delete files directory";
  return false;
}

bool OpaqueFilesCleaner::CleanStaleSessions(const ApkCacheDatabase& db) const {
  auto sessions = db.GetSessions();
  if (!sessions)
    return false;

  base::Time current_time = base::Time::Now();

  for (const Session& session : *sessions) {
    if (session.status == kSessionStatusOpen) {
      // Check if the session is expired. A session will expire if the process
      // that created it exited abnormally. For example, Play Store might be
      // killed during streaming files because of system shutdown. In this
      // situation the dead session will never be closed normally and will block
      // other sessions from being created.
      base::TimeDelta age = current_time - session.timestamp;
      if (age.InSeconds() < 0)
        LOG(WARNING) << "Session " << session.id << " is in the future";
      else if (age > kSessionMaxAge)
        LOG(WARNING) << "Session " << session.id << " expired";
      else
        continue;

      if (!db.DeleteSession(session.id))
        return false;
    }
  }

  return true;
}

bool OpaqueFilesCleaner::IsOtherSessionActive(
    const ApkCacheDatabase& db) const {
  auto sessions = db.GetSessions();
  if (!sessions)
    return true;

  for (const Session& session : *sessions) {
    if (session.status == kSessionStatusOpen) {
      LOG(INFO) << "Session " << session.id << " from " << session.source
                << " is active";
      return true;
    }
  }

  return false;
}

int64_t OpaqueFilesCleaner::OpenSession(const ApkCacheDatabase& db) const {
  Session session;
  session.source = kCacheCleanerSessionSource;
  session.timestamp = base::Time::Now();
  session.status = kSessionStatusOpen;

  return db.InsertSession(session);
}

bool OpaqueFilesCleaner::CloseSession(const ApkCacheDatabase& db,
                                      uint64_t id) const {
  return db.UpdateSessionStatus(id, kSessionStatusClosed);
}

}  // namespace apk_cache
