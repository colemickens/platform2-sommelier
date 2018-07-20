// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SMBPROVIDER_MOUNT_MANAGER_H_
#define SMBPROVIDER_MOUNT_MANAGER_H_

#include <map>
#include <memory>
#include <string>

#include <base/files/file_util.h>
#include <base/macros.h>

#include "smbprovider/constants.h"
#include "smbprovider/metadata_cache.h"

namespace base {
class TickClock;
};

namespace smbprovider {

class CredentialStore;

// MountManager maintains a mapping of open mounts and the metadata associated
// with each mount.
class MountManager {
 public:
  explicit MountManager(std::unique_ptr<CredentialStore> credential_store,
                        std::unique_ptr<base::TickClock> tick_clock);
  ~MountManager();

  // Returns true if |mount_id| is already mounted.
  bool IsAlreadyMounted(int32_t mount_id) const;

  // Returns true if |mount_root| is already mounted.
  bool IsAlreadyMounted(const std::string& mount_root) const;

  // Adds |mount_root| to the |mounts_| map and outputs the |mount_id|
  // that was assigned to this mount. Ids are >=0 and are not
  // re-used within the lifetime of this class. If |mount_root| is already
  // mounted, this returns false and |mount_id| will be unmodified. If
  // |workgroup|, |username|, and |password_fd| are provided, they will be used
  // as credentials when interacting with the mount.
  // TODO(zentaro): Review if this should have a maximum number of mounts,
  // even if it is relatively large. It may already be enforced at a higher
  // level.
  bool AddMount(const std::string& mount_root,
                const std::string& workgroup,
                const std::string& username,
                const base::ScopedFD& password_fd,
                int32_t* mount_id);

  // Adds |mount_root| to the |mounts_| map with a specific mount_id. Must not
  // be called after AddMount is called for the first time. Returns false if
  // |mount_root| is already mounted. If |workgroup| and |username| are
  // provided, they will be used as credentials when interacting with the mount.
  bool Remount(const std::string& mount_root,
               int32_t mount_id,
               const std::string& workgroup,
               const std::string& username,
               const base::ScopedFD& password_fd);

  // Returns true if |mount_id| was mounted and removes the mount.
  bool RemoveMount(int32_t mount_id);

  // Returns the number of mounts.
  size_t MountCount() const { return mounts_.size(); }

  // Uses the mount root associated with |mount_id| and appends |entry_path|
  // to form |full_path|.
  bool GetFullPath(int32_t mount_id,
                   const std::string& entry_path,
                   std::string* full_path) const;

  // Gets a pointer to the metadata cache for |mount_id|.
  bool GetMetadataCache(int32_t mount_id, MetadataCache** cache) const;

  // Uses the mount root associated with |mount_id| to remove the root path
  // from |full_path| to yield a relative path.
  std::string GetRelativePath(int32_t mount_id,
                              const std::string& full_path) const;

 private:
  // Maintains the state of a single mount. Contains the mount root path and
  // the metadata cache.
  struct MountInfo {
    MountInfo() = default;
    MountInfo(const std::string& mount_root, base::TickClock* tick_clock)
        : mount_root(mount_root) {
      cache = std::make_unique<MetadataCache>(
          tick_clock, base::TimeDelta::FromMicroseconds(
                          kMetadataCacheLifetimeMicroseconds));
    }

    MountInfo& operator=(MountInfo&& other) = default;

    std::string mount_root;
    std::unique_ptr<MetadataCache> cache;

    DISALLOW_COPY_AND_ASSIGN(MountInfo);
  };

  // Returns true if |mount_root| exists as a value in mounts_. This method is
  // only used for DCHECK to ensure that credential_store_ is in sync with
  // MountManager.
  bool ExistsInMounts(const std::string& mount_root) const;

  bool can_remount_ = true;
  std::map<int32_t, MountInfo> mounts_;
  int32_t next_mount_id_ = 0;
  std::unique_ptr<CredentialStore> credential_store_;
  std::unique_ptr<base::TickClock> tick_clock_;

  DISALLOW_COPY_AND_ASSIGN(MountManager);
};

}  // namespace smbprovider

#endif  // SMBPROVIDER_MOUNT_MANAGER_H_
