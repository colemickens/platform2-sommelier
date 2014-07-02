// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sys/types.h>

#include <string>

#include <base/files/file_path.h>

#ifndef LIBCHROMEOS_CHROMEOS_UI_UTIL_H_
#define LIBCHROMEOS_CHROMEOS_UI_UTIL_H_

namespace chromeos {
namespace ui {
namespace util {

// Converts an absolute path |path| into a base::FilePath. If |parent| is
// non-empty, |path| is rooted within it. For example, GetPath("/usr/bin/bar",
// base::FilePath("/tmp/foo")) returns base::FilePath("/tmp/foo/usr/bin/bar")).
base::FilePath GetReparentedPath(const std::string& path,
                                 const base::FilePath& parent);

// Changes the ownership of |path| to |uid|:|gid| and sets its mode to |mode|.
// Skips updating ownership when not running as root (for use in tests).
bool SetPermissions(const base::FilePath& path,
                    uid_t uid,
                    gid_t gid,
                    mode_t mode);

// Ensures that |path| exists with the requested ownership and permissions,
// creating and/or updating it if needed. Returns true on success.
bool EnsureDirectoryExists(const base::FilePath& path,
                           uid_t uid,
                           gid_t gid,
                           mode_t mode);

// Looks up the UID and GID corresponding to |user|. Returns true on success.
bool GetUserInfo(const std::string& user, uid_t* uid, gid_t* gid);

// Runs the passed-in command and arguments synchronously, returning true on
// success. On failure, the command's output is logged. The path will be
// searched for |command|.
bool Run(const char* command, const char* arg, ...);

}  // namespace util
}  // namespace ui
}  // namespace chromeos

#endif  // LIBCHROMEOS_CHROMEOS_UI_UTIL_H_
