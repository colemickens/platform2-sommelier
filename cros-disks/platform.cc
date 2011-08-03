// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cros-disks/platform.h"

#include <grp.h>
#include <pwd.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <unistd.h>

#include <base/file_util.h>
#include <base/logging.h>
#include <base/memory/scoped_ptr.h>
#include <base/stringprintf.h>

using std::string;

namespace cros_disks {

static const unsigned kFallbackGroupBufferSize = 16384;
static const unsigned kFallbackPasswordBufferSize = 16384;

Platform::Platform()
    : experimental_features_enabled_(false),
      mount_group_id_(0),
      mount_user_id_(0) {
}

Platform::~Platform() {
}

bool Platform::CreateDirectory(const string& path) const {
  if (!file_util::CreateDirectory(FilePath(path))) {
    LOG(ERROR) << "Failed to create directory '" << path << "'";
    return false;
  }
  LOG(INFO) << "Created directory '" << path << "'";
  return true;
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

bool Platform::CreateOrReuseEmptyDirectoryWithFallback(
    string* path, unsigned max_suffix_to_retry) const {
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

bool Platform::GetGroupId(const string& group_name, gid_t* group_id) const {
  long buffer_size = sysconf(_SC_GETGR_R_SIZE_MAX);
  if (buffer_size <= 0)
    buffer_size = kFallbackGroupBufferSize;

  struct group group_buffer, *group_buffer_ptr = NULL;
  scoped_array<char> buffer(new char[buffer_size]);
  getgrnam_r(group_name.c_str(), &group_buffer, buffer.get(), buffer_size,
      &group_buffer_ptr);
  if (group_buffer_ptr == NULL) {
    PLOG(WARNING) << "Failed to determine group ID of group '"
      << group_name << "'";
    return false;
  }

  if (group_id)
    *group_id = group_buffer.gr_gid;
  return true;
}

bool Platform::GetUserAndGroupId(const string& user_name,
                                 uid_t* user_id, gid_t* group_id) const {
  long buffer_size = sysconf(_SC_GETPW_R_SIZE_MAX);
  if (buffer_size <= 0)
    buffer_size = kFallbackPasswordBufferSize;

  struct passwd password_buffer, *password_buffer_ptr = NULL;
  scoped_array<char> buffer(new char[buffer_size]);
  getpwnam_r(user_name.c_str(), &password_buffer, buffer.get(), buffer_size,
      &password_buffer_ptr);
  if (password_buffer_ptr == NULL) {
    PLOG(WARNING) << "Failed to determine user and group ID of user '"
      << user_name << "'";
    return false;
  }

  if (user_id)
    *user_id = password_buffer.pw_uid;

  if (group_id)
    *group_id = password_buffer.pw_gid;

  return true;
}

bool Platform::GetOwnership(const string& path,
                            uid_t* user_id, gid_t* group_id) const {
  struct stat path_status;
  if (stat(path.c_str(), &path_status) != 0) {
    PLOG(ERROR) << "Failed to get the ownership of '" << path << "'";
    return false;
  }

  if (user_id)
    *user_id = path_status.st_uid;

  if (group_id)
    *group_id = path_status.st_gid;

  return true;
}

bool Platform::GetPermissions(const string& path, mode_t* mode) const {
  CHECK(mode) << "Invalid mode argument";

  struct stat path_status;
  if (stat(path.c_str(), &path_status) != 0) {
    PLOG(ERROR) << "Failed to get the permissions of '" << path << "'";
    return false;
  }
  *mode = path_status.st_mode;
  return true;
}

bool Platform::SetMountUser(const string& user_name) {
  return GetUserAndGroupId(user_name, &mount_user_id_, &mount_group_id_);
}

bool Platform::RemoveEmptyDirectory(const string& path) const {
  if (rmdir(path.c_str()) != 0) {
    PLOG(WARNING) << "Failed to remove directory '" << path << "'";
    return false;
  }
  return true;
}

bool Platform::SetOwnership(const string& path,
                            uid_t user_id, gid_t group_id) const {
  if (chown(path.c_str(), user_id, group_id)) {
    PLOG(ERROR) << "Failed to set ownership of '" << path
                << "' to (uid=" << user_id << ", gid=" << group_id << ")";
    return false;
  }
  return true;
}

bool Platform::SetPermissions(const string& path, mode_t mode) const {
  if (chmod(path.c_str(), mode)) {
    PLOG(ERROR) << "Failed to set permissions of '" << path
                << "' to " << base::StringPrintf("%04o", mode);
    return false;
  }
  return true;
}

bool Platform::Unmount(const string& path) const {
  if (umount(path.c_str()) != 0) {
    PLOG(ERROR) << "Failed to unmount '" << path << "'";
    return false;
  }
  LOG(INFO) << "Unmount '" << path << "'";
  return true;
}

}  // namespace cros_disks
