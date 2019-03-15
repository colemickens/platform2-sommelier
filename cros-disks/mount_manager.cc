// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Implements cros-disks::MountManager. See mount-manager.h for details.

#include "cros-disks/mount_manager.h"

#include <sys/mount.h>
#include <unistd.h>

#include <algorithm>
#include <utility>

#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/logging.h>
#include <base/stl_util.h>
#include <base/strings/string_util.h>

#include "cros-disks/mount_options.h"
#include "cros-disks/platform.h"
#include "cros-disks/uri.h"

using base::FilePath;
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
// Prefix of the mount label option.
const char kMountOptionMountLabelPrefix[] = "mountlabel=";
// Literal for mount option: "remount".
const char kMountOptionRemount[] = "remount";
// Literal for unmount option: "lazy".
const char kUnmountOptionLazy[] = "lazy";
// Maximum number of trials on creating a mount directory using
// Platform::CreateOrReuseEmptyDirectoryWithFallback().
// A value of 100 seems reasonable and enough to handle directory name
// collisions under common scenarios.
const unsigned kMaxNumMountTrials = 100;

}  // namespace

namespace cros_disks {

MountManager::MountManager(const string& mount_root,
                           Platform* platform,
                           Metrics* metrics)
    : mount_root_(mount_root), platform_(platform), metrics_(metrics) {
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
         platform_->SetPermissions(mount_root_, kMountRootDirectoryPermissions);
}

bool MountManager::StartSession() {
  return true;
}

bool MountManager::StopSession() {
  return UnmountAll();
}

bool MountManager::CanUnmount(const string& path) const {
  return CanMount(path) || IsPathImmediateChildOfParent(path, mount_root_);
}

MountErrorType MountManager::Mount(const string& source_path,
                                   const string& filesystem_type,
                                   const vector<string>& options,
                                   string* mount_path) {
  // Source is not necessary a path, but if it is let's resolve it to
  // some real underlying object.
  string real_path;
  if (Uri::IsUri(source_path) ||
      !platform_->GetRealPath(source_path, &real_path)) {
    real_path = source_path;
  }

  if (real_path.empty()) {
    LOG(ERROR) << "Failed to mount an invalid path";
    return MOUNT_ERROR_INVALID_ARGUMENT;
  }
  if (!mount_path) {
    LOG(ERROR) << "Invalid mount path argument";
    return MOUNT_ERROR_INVALID_ARGUMENT;
  }

  if (find(options.begin(), options.end(), kMountOptionRemount) ==
      options.end()) {
    return MountNewSource(real_path, filesystem_type, options, mount_path);
  } else {
    return Remount(real_path, filesystem_type, options, mount_path);
  }
}

MountErrorType MountManager::Remount(const string& source_path,
                                     const string& filesystem_type,
                                     const vector<string>& options,
                                     string* mount_path) {
  if (!GetMountPathFromCache(source_path, mount_path)) {
    LOG(WARNING) << "Path '" << source_path << "' is not mounted yet";
    return MOUNT_ERROR_PATH_NOT_MOUNTED;
  }

  vector<string> updated_options = options;
  string mount_label;
  ExtractMountLabelFromOptions(&updated_options, &mount_label);

  // Perform the underlying mount operation.
  MountOptions applied_options;
  MountErrorType error_type =
      DoMount(source_path, filesystem_type, updated_options, *mount_path,
              &applied_options);
  if (error_type != MOUNT_ERROR_NONE) {
    LOG(ERROR) << "Failed to remount path '" << source_path << "'";
    return error_type;
  }

  LOG(INFO) << "Path '" << source_path << "' on '" << *mount_path
            << "' is remounted with read_only="
            << applied_options.IsReadOnlyOptionSet();
  AddOrUpdateMountStateCache(source_path, *mount_path,
                             applied_options.IsReadOnlyOptionSet());
  return error_type;
}

MountErrorType MountManager::MountNewSource(const string& source_path,
                                            const string& filesystem_type,
                                            const vector<string>& options,
                                            string* mount_path) {
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

  vector<string> updated_options = options;
  string mount_label;
  ExtractMountLabelFromOptions(&updated_options, &mount_label);

  // Create a directory and set up its ownership/permissions for mounting
  // the source path. If an error occurs, ShouldReserveMountPathOnError()
  // is not called to reserve the mount path as a reserved mount path still
  // requires a proper mount directory.
  if (mount_path->empty()) {
    actual_mount_path = SuggestMountPath(source_path);
    if (!mount_label.empty()) {
      // Replace the basename(|actual_mount_path|) with |mount_label|.
      actual_mount_path =
          FilePath(actual_mount_path).DirName().Append(mount_label).value();
    }
  } else {
    actual_mount_path = *mount_path;
  }

  if (!IsValidMountPath(actual_mount_path)) {
    LOG(ERROR) << "Mount path '" << actual_mount_path << "' is invalid";
    return MOUNT_ERROR_INVALID_PATH;
  }

  bool mount_path_created;
  if (mount_path->empty()) {
    mount_path_created = platform_->CreateOrReuseEmptyDirectoryWithFallback(
        &actual_mount_path, kMaxNumMountTrials, GetReservedMountPaths());
  } else {
    mount_path_created =
        !IsMountPathReserved(actual_mount_path) &&
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
  MountOptions applied_options;
  MountErrorType error_type =
      DoMount(source_path, filesystem_type, updated_options, actual_mount_path,
              &applied_options);
  if (error_type == MOUNT_ERROR_NONE) {
    LOG(INFO) << "Path '" << source_path << "' is mounted to '"
              << actual_mount_path << "'";
  } else if (ShouldReserveMountPathOnError(error_type)) {
    LOG(INFO) << "Reserving mount path '" << actual_mount_path << "' for '"
              << source_path << "'";
    ReserveMountPath(actual_mount_path, error_type);
  } else {
    LOG(ERROR) << "Failed to mount path '" << source_path << "'";
    platform_->RemoveEmptyDirectory(actual_mount_path);
    return error_type;
  }

  AddOrUpdateMountStateCache(source_path, actual_mount_path,
                             applied_options.IsReadOnlyOptionSet());
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
    if (!IsMountPathInCache(path)) {                // is a mount path?
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
  MountStateMap mount_states_copy = mount_states_;
  for (const auto& mount_state : mount_states_copy) {
    if (Unmount(mount_state.first, options) != MOUNT_ERROR_NONE) {
      all_umounted = false;
    }
  }
  return all_umounted;
}

void MountManager::AddOrUpdateMountStateCache(const string& source_path,
                                              const string& mount_path,
                                              bool is_read_only) {
  MountState mount_state;
  mount_state.mount_path = mount_path;
  mount_state.is_read_only = is_read_only;
  mount_states_[source_path] = mount_state;
}

bool MountManager::GetSourcePathFromCache(const string& mount_path,
                                          string* source_path) const {
  CHECK(source_path) << "Invalid source path argument";

  for (const auto& path_pair : mount_states_) {
    if (path_pair.second.mount_path == mount_path) {
      *source_path = path_pair.first;
      return true;
    }
  }
  return false;
}

bool MountManager::GetMountPathFromCache(const string& source_path,
                                         string* mount_path) const {
  CHECK(mount_path) << "Invalid mount path argument";

  MountState mount_state;
  if (!GetMountStateFromCache(source_path, &mount_state)) {
    return false;
  }

  *mount_path = mount_state.mount_path;
  return true;
}

bool MountManager::GetMountStateFromCache(const string& source_path,
                                          MountState* mount_state) const {
  CHECK(mount_state) << "Invalid mount state argument";

  MountStateMap::const_iterator path_iterator = mount_states_.find(source_path);
  if (path_iterator == mount_states_.end())
    return false;

  *mount_state = path_iterator->second;
  return true;
}

bool MountManager::IsMountPathInCache(const string& mount_path) const {
  for (const auto& path_pair : mount_states_) {
    if (path_pair.second.mount_path == mount_path)
      return true;
  }
  return false;
}

bool MountManager::RemoveMountPathFromCache(const string& mount_path) {
  for (MountStateMap::iterator path_iterator = mount_states_.begin();
       path_iterator != mount_states_.end(); ++path_iterator) {
    if (path_iterator->second.mount_path == mount_path) {
      mount_states_.erase(path_iterator);
      return true;
    }
  }
  return false;
}

bool MountManager::IsMountPathReserved(const string& mount_path) const {
  return base::ContainsKey(reserved_mount_paths_, mount_path);
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
  for (const auto& path_pair : reserved_mount_paths_) {
    reserved_paths.insert(path_pair.first);
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

vector<MountEntry> MountManager::GetMountEntries() const {
  vector<MountEntry> mount_entries;
  for (const auto& entry : mount_states_) {
    const string& source_path = entry.first;
    const MountState& mount_state = entry.second;
    const string& mount_path = mount_state.mount_path;
    bool is_read_only = mount_state.is_read_only;
    MountErrorType error_type = GetMountErrorOfReservedMountPath(mount_path);
    mount_entries.push_back(MountEntry(error_type, source_path,
                                       GetMountSourceType(), mount_path,
                                       is_read_only));
  }
  return mount_entries;
}

bool MountManager::ExtractMountLabelFromOptions(
    vector<string>* options, std::string* mount_label) const {
  CHECK(options) << "Invalid mount options argument";
  CHECK(mount_label) << "Invalid mount label argument";

  mount_label->clear();
  bool found_mount_label = false;
  vector<string>::iterator option_iterator = options->begin();
  while (option_iterator != options->end()) {
    if (!base::StartsWith(*option_iterator, kMountOptionMountLabelPrefix,
                          base::CompareCase::INSENSITIVE_ASCII)) {
      ++option_iterator;
      continue;
    }

    found_mount_label = true;
    *mount_label =
        option_iterator->substr(strlen(kMountOptionMountLabelPrefix));
    option_iterator = options->erase(option_iterator);
  }
  return found_mount_label;
}

bool MountManager::ExtractUnmountOptions(const vector<string>& options,
                                         int* unmount_flags) const {
  CHECK(unmount_flags) << "Invalid unmount flags argument";

  *unmount_flags = 0;
  for (const auto& option : options) {
    if (option == kUnmountOptionLazy) {
      *unmount_flags |= MNT_DETACH;
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
  vector<std::string> path_components, parent_components;
  FilePath(path).StripTrailingSeparators().GetComponents(&path_components);
  FilePath(parent).StripTrailingSeparators().GetComponents(&parent_components);
  if (path_components.size() != parent_components.size() + 1)
    return false;

  if (path_components.back() == FilePath::kCurrentDirectory ||
      path_components.back() == FilePath::kParentDirectory) {
    return false;
  }

  return std::equal(parent_components.begin(), parent_components.end(),
                    path_components.begin());
}

bool MountManager::IsValidMountPath(const std::string& mount_path) const {
  return IsPathImmediateChildOfParent(mount_path, mount_root_);
}

}  // namespace cros_disks
