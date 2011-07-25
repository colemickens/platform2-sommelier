// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cros-disks/platform.h"

#include <pwd.h>
#include <sys/stat.h>
#include <unistd.h>

#include <base/logging.h>
#include <base/memory/scoped_ptr.h>
#include <base/stringprintf.h>

using std::string;

namespace cros_disks {

static const unsigned kFallbackPasswordBufferSize = 16384;

Platform::Platform() {
}

Platform::~Platform() {
}

bool Platform::CreateOrReuseEmptyDirectory(const string& path) const {
  CHECK(!path.empty()) << "Invalid path argument";

  // Reuse the target path if it already exists and is empty.
  // rmdir handles the cases when the target path exists but
  // is not empty, is already mounted or is used by some process.
  rmdir(path.c_str());
  if (mkdir(path.c_str(), S_IRWXU) != 0) {
    PLOG(ERROR) << "Failed to create directory '" << path << "'";
    return false;
  }
  return true;
}

bool Platform::CreateOrReuseEmptyDirectoryWithFallback(string* path,
    unsigned max_suffix_to_retry) const {
  CHECK(path && !path->empty()) << "Invalid path argument";

  if (CreateOrReuseEmptyDirectory(*path))
    return true;

  for (unsigned suffix = 1; suffix <= max_suffix_to_retry; ++suffix) {
    string fallback_path = base::StringPrintf("%s (%d)", path->c_str(), suffix);
    if (CreateOrReuseEmptyDirectory(fallback_path)) {
      *path = fallback_path;
      return true;
    }
  }
  return false;
}

bool Platform::GetUserAndGroupId(const string& username,
    uid_t *uid, gid_t *gid) const {
  long buffer_size = sysconf(_SC_GETPW_R_SIZE_MAX);
  if (buffer_size <= 0)
    buffer_size = kFallbackPasswordBufferSize;

  struct passwd password_buffer, *password_buffer_ptr = NULL;
  scoped_array<char> buffer(new char[buffer_size]);
  getpwnam_r(username.c_str(), &password_buffer, buffer.get(), buffer_size,
      &password_buffer_ptr);
  if (password_buffer_ptr == NULL) {
    PLOG(WARNING) << "Could not determine user and group ID of username '"
      << username << "'";
    return false;
  }

  if (uid)
    *uid = password_buffer.pw_uid;

  if (gid)
    *gid = password_buffer.pw_gid;

  return true;
}

bool Platform::RemoveEmptyDirectory(const string& path) const {
  if (rmdir(path.c_str()) != 0) {
    PLOG(WARNING) << "Failed to remove directory '" << path << "'";
    return false;
  }
  return true;
}

}  // namespace cros_disks
