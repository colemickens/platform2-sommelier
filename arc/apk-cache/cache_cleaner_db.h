// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ARC_APK_CACHE_CACHE_CLEANER_DB_H_
#define ARC_APK_CACHE_CACHE_CLEANER_DB_H_

#include <array>
#include <string>
#include <unordered_set>

namespace base {
class FilePath;
class TimeDelta;
}  // namespace base

namespace apk_cache {

// Expose files directory to old cache cleaner and unit tests.
extern const char kFilesBase[];

// Database file for testing.
extern const char kDatabaseFile[];

// Session status for testing.
extern const int32_t kSessionStatusClosed;

// File types for testing.
extern const char kFileTypeBaseApk[];

// Expose database files to old cache cleaner. Will be removed once old cache
// cleaner is removed.
extern const std::array<const char*, 4> kDatabaseFiles;

// Performs cleaning of opaque files organized by database in the APK cache
// directory. Also deletes invalid entries in the database. The path to the
// cache directory must be provided as |cache_root| parameter. Returns true if
// all the intended files and directories were successfully deleted.
bool CleanOpaqueFiles(const base::FilePath& cache_root);

}  // namespace apk_cache

#endif  // ARC_APK_CACHE_CACHE_CLEANER_DB_H_
