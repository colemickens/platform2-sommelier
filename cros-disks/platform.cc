// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cros-disks/platform.h"

#include <grp.h>
#include <pwd.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <unistd.h>

#include <memory>
#include <vector>

#include <base/files/file_util.h>
#include <base/logging.h>
#include <base/memory/free_deleter.h>
#include <base/stl_util.h>
#include <base/strings/string_util.h>
#include <base/strings/stringprintf.h>

using base::FilePath;
using std::string;
using std::unique_ptr;
using std::vector;

namespace {

const unsigned kFallbackGroupBufferSize = 16384;
const unsigned kFallbackPasswordBufferSize = 16384;

}  // namespace

namespace cros_disks {

Platform::Platform()
    : mount_group_id_(0), mount_user_id_(0), mount_user_("root") {}

bool Platform::GetRealPath(const string& path, string* real_path) const {
  CHECK(real_path) << "Invalid real_path argument";

  unique_ptr<char, base::FreeDeleter> result(realpath(path.c_str(), nullptr));
  if (!result) {
    PLOG(ERROR) << "Failed to get real path of '" << path << "'";
    return false;
  }

  *real_path = result.get();
  return true;
}

bool Platform::PathExists(const std::string& path) const {
  return base::PathExists(FilePath(path));
}

bool Platform::DirectoryExists(const std::string& path) const {
  return base::DirectoryExists(FilePath(path));
}

bool Platform::IsDirectoryEmpty(const std::string& dir) const {
  return base::IsDirectoryEmpty(FilePath(dir));
}

bool Platform::CreateDirectory(const string& path) const {
  if (!base::CreateDirectory(FilePath(path))) {
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
    string* path,
    unsigned max_suffix_to_retry,
    const std::set<std::string>& reserved_paths) const {
  CHECK(path && !path->empty()) << "Invalid path argument";

  if (!base::ContainsKey(reserved_paths, *path) &&
      CreateOrReuseEmptyDirectory(*path))
    return true;

  for (unsigned suffix = 1; suffix <= max_suffix_to_retry; ++suffix) {
    string fallback_path = GetDirectoryFallbackName(*path, suffix);
    if (!base::ContainsKey(reserved_paths, fallback_path) &&
        CreateOrReuseEmptyDirectory(fallback_path)) {
      *path = fallback_path;
      return true;
    }
  }
  return false;
}

bool Platform::CreateTemporaryDirInDir(const std::string& dir,
                                       const std::string& prefix,
                                       std::string* path) const {
  FilePath dest;
  bool result = base::CreateTemporaryDirInDir(FilePath(dir), prefix, &dest);
  if (result && path)
    *path = dest.value();
  return result;
}

int Platform::WriteFile(const std::string& file,
                        const char* data,
                        int size) const {
  return base::WriteFile(FilePath(file), data, size);
}

int Platform::ReadFile(const std::string& file, char* data, int size) const {
  return base::ReadFile(FilePath(file), data, size);
}

string Platform::GetDirectoryFallbackName(const string& path,
                                          unsigned suffix) const {
  if (!path.empty() && base::IsAsciiDigit(path[path.size() - 1]))
    return base::StringPrintf("%s (%u)", path.c_str(), suffix);

  return base::StringPrintf("%s %u", path.c_str(), suffix);
}

bool Platform::GetGroupId(const string& group_name, gid_t* group_id) const {
  long buffer_size = sysconf(_SC_GETGR_R_SIZE_MAX);  // NOLINT(runtime/int)
  if (buffer_size <= 0)
    buffer_size = kFallbackGroupBufferSize;

  group group_buffer, *group_buffer_ptr = nullptr;
  vector<char> buffer(buffer_size);
  getgrnam_r(group_name.c_str(), &group_buffer, buffer.data(), buffer_size,
             &group_buffer_ptr);
  if (group_buffer_ptr == nullptr) {
    PLOG(WARNING) << "Failed to determine group ID of group '" << group_name
                  << "'";
    return false;
  }

  if (group_id)
    *group_id = group_buffer.gr_gid;
  return true;
}

bool Platform::GetUserAndGroupId(const string& user_name,
                                 uid_t* user_id,
                                 gid_t* group_id) const {
  long buffer_size = sysconf(_SC_GETPW_R_SIZE_MAX);  // NOLINT(runtime/int)
  if (buffer_size <= 0)
    buffer_size = kFallbackPasswordBufferSize;

  passwd password_buffer, *password_buffer_ptr = nullptr;
  vector<char> buffer(buffer_size);
  getpwnam_r(user_name.c_str(), &password_buffer, buffer.data(), buffer_size,
             &password_buffer_ptr);
  if (password_buffer_ptr == nullptr) {
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
                            uid_t* user_id,
                            gid_t* group_id) const {
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
  if (GetUserAndGroupId(user_name, &mount_user_id_, &mount_group_id_)) {
    mount_user_ = user_name;
    return true;
  }
  return false;
}

bool Platform::RemoveEmptyDirectory(const string& path) const {
  if (rmdir(path.c_str()) != 0) {
    PLOG(WARNING) << "Failed to remove directory '" << path << "'";
    return false;
  }
  return true;
}

bool Platform::SetOwnership(const string& path,
                            uid_t user_id,
                            gid_t group_id) const {
  if (chown(path.c_str(), user_id, group_id)) {
    PLOG(ERROR) << "Failed to set ownership of '" << path
                << "' to (uid=" << user_id << ", gid=" << group_id << ")";
    return false;
  }
  return true;
}

bool Platform::SetPermissions(const string& path, mode_t mode) const {
  if (chmod(path.c_str(), mode)) {
    PLOG(ERROR) << "Failed to set permissions of '" << path << "' to "
                << base::StringPrintf("%04o", mode);
    return false;
  }
  return true;
}

bool Platform::Unmount(const string& path, int flags) const {
  if (umount2(path.c_str(), flags) != 0) {
    PLOG(ERROR) << "Failed to unmount '" << path << "' with flags " << flags;
    return false;
  }
  LOG(INFO) << "Unmount '" << path << "' with flags " << flags;
  return true;
}

bool Platform::Mount(const string& source_path,
                     const string& target_path,
                     const string& filesystem_type,
                     unsigned long options,  // NOLINT(runtime/int)
                     const std::string& data) const {
  if (mount(source_path.c_str(), target_path.c_str(), filesystem_type.c_str(),
            options, data.c_str()) != 0) {
    PLOG(ERROR) << "Failed to mount '" << source_path << "' '" << target_path
                << "' '" << filesystem_type << "' " << options << " '" << data
                << "'";
    return false;
  }
  LOG(INFO) << "Mount '" << source_path << "' '" << target_path << "' '"
            << filesystem_type << "' " << options << " '" << data << "'";
  return true;
}

}  // namespace cros_disks
