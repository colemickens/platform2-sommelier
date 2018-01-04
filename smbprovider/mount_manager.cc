// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "smbprovider/mount_manager.h"

#include "smbprovider/smbprovider_helper.h"

namespace smbprovider {

MountManager::MountManager() = default;

MountManager::~MountManager() = default;

bool MountManager::IsAlreadyMounted(int32_t mount_id) const {
  return mounts_.count(mount_id) > 0;
}

int32_t MountManager::AddMount(const std::string& mount_root) {
  DCHECK(!IsAlreadyMounted(next_mount_id_));

  mounts_[next_mount_id_] = mount_root;
  return next_mount_id_++;
}

bool MountManager::RemoveMount(int32_t mount_id) {
  // Returns true if |mount_id| was in the map.
  return mounts_.erase(mount_id) != 0;
}

bool MountManager::GetFullPath(int32_t mount_id,
                               const std::string& entry_path,
                               std::string* full_path) const {
  DCHECK(full_path);

  auto mount_iter = mounts_.find(mount_id);
  if (mount_iter == mounts_.end()) {
    return false;
  }

  *full_path = AppendPath(mount_iter->second, entry_path);
  return true;
}

}  // namespace smbprovider
