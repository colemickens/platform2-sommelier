// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "smbprovider/mount_tracker.h"

namespace smbprovider {

MountTracker::MountTracker() = default;

MountTracker::~MountTracker() = default;

bool MountTracker::IsAlreadyMounted(int32_t mount_id) const {
  auto mount_iter = mounts_.Find(mount_id);
  if (mount_iter == mounts_.End()) {
    return false;
  }

  // Check if |mounted_share_paths_| and |mounts_| are in sync.
  DCHECK(ExistsInMountedSharePaths(mount_iter->second.mount_root));
  return true;
}

bool MountTracker::IsAlreadyMounted(const std::string& mount_root) const {
  bool has_credential = ExistsInMountedSharePaths(mount_root);
  if (!has_credential) {
    DCHECK(!ExistsInMounts(mount_root));
    return false;
  }

  DCHECK(ExistsInMounts(mount_root));
  return true;
}

bool MountTracker::ExistsInMounts(const std::string& mount_root) const {
  for (auto mount_iter = mounts_.Begin(); mount_iter != mounts_.End();
       ++mount_iter) {
    if (mount_iter->second.mount_root == mount_root) {
      return true;
    }
  }

  return false;
}

bool MountTracker::ExistsInMountedSharePaths(
    const std::string& mount_root) const {
  return mounted_share_paths_.count(mount_root) == 1;
}

}  // namespace smbprovider
