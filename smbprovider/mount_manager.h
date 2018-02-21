// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SMBPROVIDER_MOUNT_MANAGER_H_
#define SMBPROVIDER_MOUNT_MANAGER_H_

#include <map>
#include <string>

#include <base/macros.h>

namespace smbprovider {

// MountManager maintains a mapping of open mounts and the metadata associated
// with each mount.
class MountManager {
 public:
  MountManager();
  ~MountManager();

  // Returns true if |mount_id| is already mounted.
  bool IsAlreadyMounted(int32_t mount_id) const;

  // Adds |mount_root| to the |mounts_| map and returns the mount id
  // that was assigned to this mount. Returned ids are >=0 and are not
  // re-used within the lifetime of this class.
  // TODO(zentaro): Review if this should have a maximum number of mounts,
  // even if it is relatively large. It may already be enforced at a higher
  // level.
  int32_t AddMount(const std::string& mount_root);

  // Returns true if |mount_id| was mounted and removes the mount.
  bool RemoveMount(int32_t mount_id);

  // Returns the number of mounts.
  size_t MountCount() const { return mounts_.size(); }

  // Uses the mount root associated with |mount_id| and appends |entry_path|
  // to form |full_path|.
  bool GetFullPath(int32_t mount_id,
                   const std::string& entry_path,
                   std::string* full_path) const;

  // Uses the mount root associated with |mount_id| to remove the root path
  // from |full_path| to yield a relative path.
  std::string GetRelativePath(int32_t mount_id,
                              const std::string& full_path) const;

 private:
  std::map<int32_t, std::string> mounts_;
  int32_t next_mount_id_ = 0;

  DISALLOW_COPY_AND_ASSIGN(MountManager);
};

}  // namespace smbprovider

#endif  // SMBPROVIDER_MOUNT_MANAGER_H_
