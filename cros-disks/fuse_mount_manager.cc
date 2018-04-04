// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cros-disks/fuse_mount_manager.h"

#include <utility>

#include <base/files/file_path.h>
#include <base/logging.h>

#include "cros-disks/fuse_helper.h"
#include "cros-disks/platform.h"

using base::FilePath;
using std::string;
using std::vector;

namespace cros_disks {

FUSEMountManager::FUSEMountManager(const string& mount_root,
                                   Platform* platform,
                                   Metrics* metrics)
    : MountManager(mount_root, platform, metrics) {}

FUSEMountManager::~FUSEMountManager() {}

bool FUSEMountManager::Initialize() {
  if (!MountManager::Initialize())
    return false;

  // Register specific FUSE mount helpers here.

  return true;
}

MountErrorType FUSEMountManager::DoMount(const string& source,
                                         const string& fuse_type,
                                         const vector<string>& options,
                                         const string& mount_path,
                                         MountOptions* applied_options) {
  CHECK(!mount_path.empty()) << "Invalid mount path argument";

  const FUSEHelper* selected_helper = nullptr;
  for (const auto& helper : helpers_) {
    if (helper->CanMount(source)) {
      selected_helper = helper.get();
      break;
    }
  }

  if (!selected_helper) {
    LOG(ERROR) << "Can't find sutable FUSE module for type '" << fuse_type
               << "' and source '" << source << "'";
    return MOUNT_ERROR_UNKNOWN_FILESYSTEM;
  }

  return selected_helper->Mount(source, mount_path, options);
}

MountErrorType FUSEMountManager::DoUnmount(const string& path,
                                           const vector<string>& options) {
  // DoUnmount() is always called with |path| being the mount path.
  CHECK(!path.empty()) << "Invalid path argument";
  if (platform()->Unmount(path)) {
    return MOUNT_ERROR_NONE;
  }
  return MOUNT_ERROR_UNKNOWN;
}

bool FUSEMountManager::CanMount(const string& source) const {
  for (const auto& helper : helpers_) {
    if (helper->CanMount(source))
      return true;
  }
  return false;
}

string FUSEMountManager::SuggestMountPath(const string& source) const {
  for (const auto& helper : helpers_) {
    if (helper->CanMount(source))
      return FilePath(mount_root())
          .Append(helper->GetTargetSuffix(source))
          .value();
  }
  FilePath base_name = FilePath(source).BaseName();
  return FilePath(mount_root()).Append(base_name).value();
}

void FUSEMountManager::RegisterHelper(std::unique_ptr<FUSEHelper> helper) {
  helpers_.push_back(std::move(helper));
}

}  // namespace cros_disks
