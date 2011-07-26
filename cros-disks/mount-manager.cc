// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cros-disks/mount-manager.h"

#include <sys/mount.h>
#include <unistd.h>

#include <algorithm>

#include <base/file_path.h>
#include <base/logging.h>

#include "cros-disks/platform.h"

using std::map;
using std::pair;
using std::string;
using std::vector;

namespace cros_disks {

static const mode_t kMountRootDirectoryPermissions =
    S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH;
static const char kUnmountOptionForce[] = "force";
static const unsigned kMaxNumMountTrials = 10000;

MountManager::MountManager(const string& mount_root, Platform* platform)
    : mount_root_(mount_root),
      platform_(platform) {
  CHECK(!mount_root_.empty()) << "Invalid mount root directory";
  CHECK(platform_) << "Invalid platform object";
}

MountManager::~MountManager() {
  UnmountAll();
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
    return kMountErrorInvalidArgument;
  }
  if (!mount_path) {
    LOG(ERROR) << "Invalid mount path argument";
    return kMountErrorInvalidArgument;
  }

  string actual_mount_path;
  if (GetMountPathFromCache(source_path, &actual_mount_path)) {
    LOG(WARNING) << "Path '" << source_path << "' is already mounted to '"
                 << actual_mount_path << "'";
    // TODO(benchan): Should probably compare filesystem type and mount options
    //                with those used in previous mount.
    if (mount_path->empty() || *mount_path == actual_mount_path) {
      *mount_path = actual_mount_path;
      return kMountErrorNone;
    } else {
      return kMountErrorPathAlreadyMounted;
    }
  }

  bool mount_path_created;
  if (mount_path->empty()) {
    actual_mount_path = SuggestMountPath(source_path);
    mount_path_created =
        platform_->CreateOrReuseEmptyDirectoryWithFallback(&actual_mount_path,
                                                           kMaxNumMountTrials);
  } else {
    actual_mount_path = *mount_path;
    mount_path_created =
        platform_->CreateOrReuseEmptyDirectory(actual_mount_path);
  }
  if (!mount_path_created) {
    LOG(ERROR) << "Failed to create directory '" << actual_mount_path
               << "' to mount '" << source_path << "'";
    return kMountErrorDirectoryCreationFailed;
  }

  MountErrorType error_type =
      DoMount(source_path, filesystem_type, options, actual_mount_path);
  if (error_type == kMountErrorNone) {
    LOG(INFO) << "Path '" << source_path << "' is mounted to '"
              << actual_mount_path << "'";
    AddMountPathToCache(source_path, actual_mount_path);
    *mount_path = actual_mount_path;
  } else {
    LOG(ERROR) << "Failed to mount path '" << source_path << "'";
    platform_->RemoveEmptyDirectory(actual_mount_path);
  }
  return error_type;
}

MountErrorType MountManager::Unmount(const string& path,
                                     const vector<string>& options) {
  if (path.empty()) {
    LOG(ERROR) << "Failed to unmount an empty path";
    return kMountErrorInvalidArgument;
  }

  // Deteremine whether the path is a source path or a mount path.
  string mount_path;
  if (!GetMountPathFromCache(path, &mount_path)) {  // is a source path?
    if (!IsMountPathInCache(path)) {  // is a mount path?
      LOG(ERROR) << "Path '" << path << "' is not mounted";
      return kMountErrorPathNotMounted;
    }
    mount_path = path;
  }

  MountErrorType error_type = DoUnmount(mount_path, options);
  if (error_type == kMountErrorNone) {
    LOG(INFO) << "Unmounted '" << mount_path << "'";
    RemoveMountPathFromCache(mount_path);
    platform_->RemoveEmptyDirectory(mount_path);
  } else {
    LOG(ERROR) << "Failed to unmount '" << mount_path << "'";
  }
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
    if (Unmount(path_iterator->first, options) != kMountErrorNone) {
      all_umounted = false;
    }
  }
  return all_umounted;
}

bool MountManager::AddMountPathToCache(const string& source_path,
                                       const string& mount_path) {
  pair<MountPathMap::iterator, bool> result =
      mount_paths_.insert(std::make_pair(source_path, mount_path));
  return result.second;
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
