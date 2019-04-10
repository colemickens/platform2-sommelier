// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cros-disks/platform.h"

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
#include <brillo/userdb_utils.h>

namespace cros_disks {

Platform::Platform()
    : mount_group_id_(0), mount_user_id_(0), mount_user_("root") {}

bool Platform::GetRealPath(const std::string& path,
                           std::string* real_path) const {
  CHECK(real_path) << "Invalid real_path argument";

  std::unique_ptr<char, base::FreeDeleter> result(
      realpath(path.c_str(), nullptr));
  if (!result) {
    PLOG(ERROR) << "Failed to get real path of '" << path << "'";
    return false;
  }

  *real_path = result.get();
  return true;
}

bool Platform::PathExists(const std::string& path) const {
  return base::PathExists(base::FilePath(path));
}

bool Platform::DirectoryExists(const std::string& path) const {
  return base::DirectoryExists(base::FilePath(path));
}

bool Platform::IsDirectoryEmpty(const std::string& dir) const {
  return base::IsDirectoryEmpty(base::FilePath(dir));
}

bool Platform::CreateDirectory(const std::string& path) const {
  if (!base::CreateDirectory(base::FilePath(path))) {
    LOG(ERROR) << "Failed to create directory '" << path << "'";
    return false;
  }
  LOG(INFO) << "Created directory '" << path << "'";
  return true;
}

bool Platform::CreateOrReuseEmptyDirectory(const std::string& path) const {
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
    std::string* path,
    unsigned max_suffix_to_retry,
    const std::set<std::string>& reserved_paths) const {
  CHECK(path && !path->empty()) << "Invalid path argument";

  if (!base::ContainsKey(reserved_paths, *path) &&
      CreateOrReuseEmptyDirectory(*path))
    return true;

  for (unsigned suffix = 1; suffix <= max_suffix_to_retry; ++suffix) {
    std::string fallback_path = GetDirectoryFallbackName(*path, suffix);
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
  base::FilePath dest;
  bool result =
      base::CreateTemporaryDirInDir(base::FilePath(dir), prefix, &dest);
  if (result && path)
    *path = dest.value();
  return result;
}

int Platform::WriteFile(const std::string& file,
                        const char* data,
                        int size) const {
  return base::WriteFile(base::FilePath(file), data, size);
}

int Platform::ReadFile(const std::string& file, char* data, int size) const {
  return base::ReadFile(base::FilePath(file), data, size);
}

std::string Platform::GetDirectoryFallbackName(const std::string& path,
                                               unsigned suffix) const {
  if (!path.empty() && base::IsAsciiDigit(path[path.size() - 1]))
    return base::StringPrintf("%s (%u)", path.c_str(), suffix);

  return base::StringPrintf("%s %u", path.c_str(), suffix);
}

bool Platform::GetGroupId(const std::string& group_name,
                          gid_t* group_id) const {
  return brillo::userdb::GetGroupInfo(group_name, group_id);
}

bool Platform::GetUserAndGroupId(const std::string& user_name,
                                 uid_t* user_id,
                                 gid_t* group_id) const {
  return brillo::userdb::GetUserInfo(user_name, user_id, group_id);
}

bool Platform::GetOwnership(const std::string& path,
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

bool Platform::GetPermissions(const std::string& path, mode_t* mode) const {
  CHECK(mode) << "Invalid mode argument";

  struct stat path_status;
  if (stat(path.c_str(), &path_status) != 0) {
    PLOG(ERROR) << "Failed to get the permissions of '" << path << "'";
    return false;
  }
  *mode = path_status.st_mode;
  return true;
}

bool Platform::SetMountUser(const std::string& user_name) {
  if (GetUserAndGroupId(user_name, &mount_user_id_, &mount_group_id_)) {
    mount_user_ = user_name;
    return true;
  }
  return false;
}

bool Platform::RemoveEmptyDirectory(const std::string& path) const {
  if (rmdir(path.c_str()) != 0) {
    PLOG(WARNING) << "Failed to remove directory '" << path << "'";
    return false;
  }
  return true;
}

bool Platform::SetOwnership(const std::string& path,
                            uid_t user_id,
                            gid_t group_id) const {
  if (chown(path.c_str(), user_id, group_id)) {
    PLOG(ERROR) << "Failed to set ownership of '" << path
                << "' to (uid=" << user_id << ", gid=" << group_id << ")";
    return false;
  }
  return true;
}

bool Platform::SetPermissions(const std::string& path, mode_t mode) const {
  if (chmod(path.c_str(), mode)) {
    PLOG(ERROR) << "Failed to set permissions of '" << path << "' to "
                << base::StringPrintf("%04o", mode);
    return false;
  }
  return true;
}

MountErrorType Platform::Unmount(const std::string& path, int flags) const {
  error_t error = 0;
  if (umount2(path.c_str(), flags) != 0) {
    error = errno;
    PLOG(ERROR) << "Failed to unmount '" << path << "' with flags " << flags;
  } else {
    LOG(INFO) << "Unmount '" << path << "' with flags " << flags;
  }
  switch (error) {
    case 0:
      return MountErrorType::MOUNT_ERROR_NONE;
    case ENOENT:
      return MountErrorType::MOUNT_ERROR_PATH_NOT_MOUNTED;
    case EPERM:
      return MountErrorType::MOUNT_ERROR_INSUFFICIENT_PERMISSIONS;
    case EBUSY:
      return MountErrorType::MOUNT_ERROR_PATH_ALREADY_MOUNTED;
    default:
      return MountErrorType::MOUNT_ERROR_UNKNOWN;
  }
}

MountErrorType Platform::Mount(const std::string& source_path,
                               const std::string& target_path,
                               const std::string& filesystem_type,
                               uint64_t options,
                               const std::string& data) const {
  error_t error = 0;
  if (mount(source_path.c_str(), target_path.c_str(), filesystem_type.c_str(),
            options, data.c_str()) != 0) {
    error = errno;
    PLOG(ERROR) << "Failed to mount '" << source_path << "' '" << target_path
                << "' '" << filesystem_type << "' " << options << " '" << data
                << "'";
  } else {
    LOG(INFO) << "Mount '" << source_path << "' '" << target_path << "' '"
              << filesystem_type << "' " << options << " '" << data << "'";
  }
  switch (error) {
    case 0:
      return MountErrorType::MOUNT_ERROR_NONE;
    case ENODEV:
      return MountErrorType::MOUNT_ERROR_UNSUPPORTED_FILESYSTEM;
    case ENOENT:
    case ENOTBLK:
    case ENOTDIR:
      return MountErrorType::MOUNT_ERROR_INVALID_PATH;
    case EPERM:
      return MountErrorType::MOUNT_ERROR_INSUFFICIENT_PERMISSIONS;
    default:
      return MountErrorType::MOUNT_ERROR_UNKNOWN;
  }
}

}  // namespace cros_disks
