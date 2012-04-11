// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Implements cros-disks::MountManager. See mount-manager.h for details.

#include "cros-disks/mount-manager.h"

#include <sys/mount.h>
#include <unistd.h>

#include <algorithm>
#include <utility>

#include <base/file_path.h>
#include <base/logging.h>
#include <base/stl_util.h>

#include "cros-disks/platform.h"

using std::pair;
using std::set;
using std::string;
using std::vector;

namespace {

// Permissions to set on the mount root directory (u+rwx,og+rx).
const mode_t kMountRootDirectoryPermissions =
    S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH;
// Permissions to set on the mount directory (u+rwx,g+rwx).
const mode_t kMountDirectoryPermissions = S_IRWXU | S_IRWXG;
// Literal for unmount option: "force".
const char kUnmountOptionForce[] = "force";
// Maximum number of trials on creating a mount directory using
// Platform::CreateOrReuseEmptyDirectoryWithFallback().
// A value of 100 seems reasonable and enough to handle directory name
// collisions under common scenarios.
const unsigned kMaxNumMountTrials = 100;

}  // namespace

namespace cros_disks {

MountManager::MountManager(const string& mount_root, Platform* platform,
                           Metrics* metrics)
    : mount_root_(mount_root),
      platform_(platform),
      metrics_(metrics) {
  CHECK(!mount_root_.empty()) << "Invalid mount root directory";
  CHECK(platform_) << "Invalid platform object";
  CHECK(metrics_) << "Invalid metrics object";
}

MountManager::~MountManager() {
  // UnmountAll() should be called from a derived class instead of this base
  // class as UnmountAll() eventually calls DoUnmount(), which is a pure
  // virtual function and may crash when being invoked here.
}

bool MountManager::Initialize() {
  return platform_->CreateDirectory(mount_root_) &&
         platform_->SetOwnership(mount_root_, getuid(), getgid()) &&
         platform_->SetPermissions(mount_root_,
                                   kMountRootDirectoryPermissions);
}

bool MountManager::StartSession(const string& user) {
  return true;
}

bool MountManager::StopSession(const string& user) {
  return true;
}

bool MountManager::CanUnmount(const string& path) const {
  return CanMount(path) || IsPathImmediateChildOfParent(path, mount_root_);
}

MountErrorType MountManager::Mount(const string& source_path,
                                   const string& filesystem_type,
                                   const vector<string>& options,
                                   string *mount_path) {
  if (source_path.empty()) {
    LOG(ERROR) << "Failed to mount an empty path";
    return MOUNT_ERROR_INVALID_ARGUMENT;
  }
  if (!mount_path) {
    LOG(ERROR) << "Invalid mount path argument";
    return MOUNT_ERROR_INVALID_ARGUMENT;
  }

  string actual_mount_path;
  if (GetMountPathFromCache(source_path, &actual_mount_path)) {
    LOG(WARNING) << "Path '" << source_path << "' is already mounted to '"
                 << actual_mount_path << "'";
    // TODO(benchan): Should probably compare filesystem type and mount options
    //                with those used in previous mount.
    if (mount_path->empty() || *mount_path == actual_mount_path) {
      *mount_path = actual_mount_path;
      return GetMountErrorOfReservedMountPath(actual_mount_path);
    } else {
      return MOUNT_ERROR_PATH_ALREADY_MOUNTED;
    }
  }

  // Create a directory and set up its ownership/permissions for mounting
  // the source path. If an error occurs, ShouldReserveMountPathOnError()
  // is not called to reserve the mount path as a reserved mount path still
  // requires a proper mount directory.
  bool mount_path_created;
  if (mount_path->empty()) {
    actual_mount_path = SuggestMountPath(source_path);
    mount_path_created = platform_->CreateOrReuseEmptyDirectoryWithFallback(
        &actual_mount_path, kMaxNumMountTrials, GetReservedMountPaths());
  } else {
    actual_mount_path = *mount_path;
    mount_path_created = !IsMountPathReserved(actual_mount_path) &&
        platform_->CreateOrReuseEmptyDirectory(actual_mount_path);
  }
  if (!mount_path_created) {
    LOG(ERROR) << "Failed to create directory '" << actual_mount_path
               << "' to mount '" << source_path << "'";
    return MOUNT_ERROR_DIRECTORY_CREATION_FAILED;
  }

  if (!platform_->SetOwnership(actual_mount_path, getuid(),
                               platform_->mount_group_id()) ||
      !platform_->SetPermissions(actual_mount_path,
                                 kMountDirectoryPermissions)) {
    LOG(ERROR) << "Failed to set ownership and permissions of directory '"
               << actual_mount_path << "' to mount '" << source_path << "'";
    platform_->RemoveEmptyDirectory(actual_mount_path);
    return MOUNT_ERROR_DIRECTORY_CREATION_FAILED;
  }

  // Perform the underlying mount operation. If an error occurs,
  // ShouldReserveMountPathOnError() is called to check if the mount path
  // should be reserved.
  MountErrorType error_type =
      DoMount(source_path, filesystem_type, options, actual_mount_path);
  if (error_type == MOUNT_ERROR_NONE) {
    LOG(INFO) << "Path '" << source_path << "' is mounted to '"
              << actual_mount_path << "'";
  } else if (ShouldReserveMountPathOnError(error_type)) {
    LOG(INFO) << "Reserving mount path '" << actual_mount_path
              << "' for '" << source_path << "'";
    ReserveMountPath(actual_mount_path, error_type);
  } else {
    LOG(ERROR) << "Failed to mount path '" << source_path << "'";
    platform_->RemoveEmptyDirectory(actual_mount_path);
    return error_type;
  }

  AddMountPathToCache(source_path, actual_mount_path);
  *mount_path = actual_mount_path;
  return error_type;
}

MountErrorType MountManager::Unmount(const string& path,
                                     const vector<string>& options) {
  if (path.empty()) {
    LOG(ERROR) << "Failed to unmount an empty path";
    return MOUNT_ERROR_INVALID_ARGUMENT;
  }

  // Determine whether the path is a source path or a mount path.
  string mount_path;
  if (!GetMountPathFromCache(path, &mount_path)) {  // is a source path?
    if (!IsMountPathInCache(path)) {  // is a mount path?
      LOG(ERROR) << "Path '" << path << "' is not mounted";
      return MOUNT_ERROR_PATH_NOT_MOUNTED;
    }
    mount_path = path;
  }

  MountErrorType error_type = MOUNT_ERROR_NONE;
  if (IsMountPathReserved(mount_path)) {
    LOG(INFO) << "Removing mount path '" << mount_path
              << "' from the reserved list";
    UnreserveMountPath(mount_path);
  } else {
    error_type = DoUnmount(mount_path, options);
    if (error_type == MOUNT_ERROR_NONE) {
      LOG(INFO) << "Unmounted '" << mount_path << "'";
    } else {
      LOG(ERROR) << "Failed to unmount '" << mount_path << "'";
      return error_type;
    }
  }

  RemoveMountPathFromCache(mount_path);
  platform_->RemoveEmptyDirectory(mount_path);
  return error_type;
}

bool MountManager::UnmountAll() {
  bool all_umounted = true;
  vector<string> options;
  // Make a copy of the mount path cache before iterating through it
  // as Unmount modifies the cache.
  MountPathMap mount_paths_copy = mount_paths_;
  for (MountPathMap::const_iterator path_iterator = mount_paths_copy.begin();
       path_iterator != mount_paths_copy.end(); ++path_iterator) {
    if (Unmount(path_iterator->first, options) != MOUNT_ERROR_NONE) {
      all_umounted = false;
    }
  }
  return all_umounted;
}

bool MountManager::AddMountPathToCache(const string& source_path,
                                       const string& mount_path) {
  return mount_paths_.insert(std::make_pair(source_path, mount_path)).second;
}

bool MountManager::GetSourcePathFromCache(const string& mount_path,
                                          string* source_path) const {
  CHECK(source_path) << "Invalid source path argument";

  for (MountPathMap::const_iterator path_iterator = mount_paths_.begin();
       path_iterator != mount_paths_.end(); ++path_iterator) {
    if (path_iterator->second == mount_path) {
      *source_path = path_iterator->first;
      return true;
    }
  }
  return false;
}

bool MountManager::GetMountPathFromCache(const string& source_path,
                                         string* mount_path) const {
  CHECK(mount_path) << "Invalid mount path argument";

  MountPathMap::const_iterator path_iterator = mount_paths_.find(source_path);
  if (path_iterator == mount_paths_.end())
    return false;

  *mount_path = path_iterator->second;
  return true;
}

bool MountManager::IsMountPathInCache(const string& mount_path) const {
  for (MountPathMap::const_iterator path_iterator = mount_paths_.begin();
       path_iterator != mount_paths_.end(); ++path_iterator) {
    if (path_iterator->second == mount_path)
      return true;
  }
  return false;
}

bool MountManager::RemoveMountPathFromCache(const string& mount_path) {
  for (MountPathMap::iterator path_iterator = mount_paths_.begin();
       path_iterator != mount_paths_.end(); ++path_iterator) {
    if (path_iterator->second == mount_path) {
      mount_paths_.erase(path_iterator);
      return true;
    }
  }
  return false;
}

bool MountManager::IsMountPathReserved(const string& mount_path) const {
  return ContainsKey(reserved_mount_paths_, mount_path);
}

MountErrorType MountManager::GetMountErrorOfReservedMountPath(
    const string& mount_path) const {
  ReservedMountPathMap::const_iterator path_iterator =
      reserved_mount_paths_.find(mount_path);
  if (path_iterator != reserved_mount_paths_.end()) {
    return path_iterator->second;
  }
  return MOUNT_ERROR_NONE;
}

set<string> MountManager::GetReservedMountPaths() const {
  set<string> reserved_paths;
  for (ReservedMountPathMap::const_iterator
       path_iterator = reserved_mount_paths_.begin();
       path_iterator != reserved_mount_paths_.end();
       ++path_iterator) {
    reserved_paths.insert(path_iterator->first);
  }
  return reserved_paths;
}

void MountManager::ReserveMountPath(const string& mount_path,
                                    MountErrorType error_type) {
  reserved_mount_paths_.insert(std::make_pair(mount_path, error_type));
}

void MountManager::UnreserveMountPath(const string& mount_path) {
  reserved_mount_paths_.erase(mount_path);
}

bool MountManager::ExtractUnmountOptions(const vector<string>& options,
                                         int *unmount_flags) const {
  CHECK(unmount_flags) << "Invalid unmount flags argument";

  *unmount_flags = 0;
  for (vector<string>::const_iterator option_iterator = options.begin();
       option_iterator != options.end(); ++option_iterator) {
    const string& option = *option_iterator;
    if (option == kUnmountOptionForce) {
      *unmount_flags |= MNT_FORCE;
    } else {
      LOG(ERROR) << "Got unsupported unmount option: " << option;
      return false;
    }
  }
  return true;
}

bool MountManager::ShouldReserveMountPathOnError(
    MountErrorType error_type) const {
  return false;
}

bool MountManager::IsPathImmediateChildOfParent(const string& path,
                                                const string& parent) const {
  vector<FilePath::StringType> path_components, parent_components;
  FilePath(path).StripTrailingSeparators().GetComponents(&path_components);
  FilePath(parent).StripTrailingSeparators().GetComponents(&parent_components);
  if (path_components.size() != parent_components.size() + 1)
    return false;

  return std::equal(parent_components.begin(), parent_components.end(),
                    path_components.begin());
}

}  // namespace cros_disks
