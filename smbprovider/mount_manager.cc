// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "smbprovider/mount_manager.h"

#include <algorithm>

#include <base/strings/string_util.h>

#include "smbprovider/smbprovider_helper.h"

namespace smbprovider {

MountManager::MountManager() = default;

MountManager::~MountManager() = default;

bool MountManager::IsAlreadyMounted(int32_t mount_id) const {
  return mounts_.count(mount_id) > 0;
}

int32_t MountManager::AddMount(const std::string& mount_root) {
  DCHECK(!IsAlreadyMounted(next_mount_id_));

  can_remount_ = false;
  mounts_[next_mount_id_] = mount_root;
  return next_mount_id_++;
}

void MountManager::Remount(const std::string& mount_root, int32_t mount_id) {
  DCHECK(can_remount_);
  DCHECK(!IsAlreadyMounted(mount_id));
  DCHECK_GE(mount_id, 0);

  mounts_[mount_id] = mount_root;
  next_mount_id_ = std::max(next_mount_id_, mount_id) + 1;
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

std::string MountManager::GetRelativePath(int32_t mount_id,
                                          const std::string& full_path) const {
  auto mount_iter = mounts_.find(mount_id);
  DCHECK(mount_iter != mounts_.end());

  DCHECK(StartsWith(full_path, mount_iter->second,
                    base::CompareCase::INSENSITIVE_ASCII));

  return full_path.substr(mount_iter->second.length());
}

}  // namespace smbprovider
