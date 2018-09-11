// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SMBPROVIDER_MOUNT_TRACKER_H_
#define SMBPROVIDER_MOUNT_TRACKER_H_

#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#include <base/callback.h>
#include <base/macros.h>
#include <base/time/tick_clock.h>
#include <libpasswordprovider/password.h>

#include "smbprovider/constants.h"
#include "smbprovider/id_map.h"
#include "smbprovider/metadata_cache.h"
#include "smbprovider/samba_interface.h"
#include "smbprovider/smb_credential.h"

namespace smbprovider {

class MountTracker {
 public:
  explicit MountTracker(std::unique_ptr<base::TickClock> tick_clock);
  ~MountTracker();

  // Returns true if |mount_id| is already mounted.
  bool IsAlreadyMounted(int32_t mount_id) const;

  // Returns true if |mount_root| is already mounted.
  bool IsAlreadyMounted(const std::string& mount_root) const;

  // Adds |mount_root| to the |mounts_| map and adds SambaInterfaceId to
  // |samba_interface_map_|. Also adds |mount_root| to |mounted_root_shares_|.
  // Ids are >=0 and are not re-used within the lifetime of this class. Returns
  // false if |mount_root| already exists in |mounted_share_paths_| and
  // |mount_id| will be unmodified.
  // TODO(zentaro): Review if this should have a maximum number of mounts,
  // even if it is relatively large. It may already be enforced at a higher
  // level.
  bool AddMount(const std::string& mount_root,
                SmbCredential credential,
                std::unique_ptr<SambaInterface> samba_interface,
                int32_t* mount_id);

  // Adds |mount_root| to the |mounts_| map with a specific |mount_id| and adds
  // SambaInterfaceId to |samba_interface_map_|. Also adds |mount_root| to
  // |mounted_root_shares_|. Returns false if |mount_root| already
  // exists in |mounted_share_paths_| or |mount_id| exists in |mounts_|.
  bool AddMountWithId(const std::string& mount_root,
                      SmbCredential credential,
                      std::unique_ptr<SambaInterface> samba_interface,
                      int32_t mount_id);

  // Returns true if |mount_id| was mounted and removes the mount. Returns false
  // if |mount_id| does not exist in |mounts_|.
  bool RemoveMount(int32_t mount_id);

  // Returns the number of mounts.
  size_t MountCount() const { return mounts_.Count(); }

 private:
  // Maintains the state of a single mount. Contains the mount root path and
  // the metadata cache.
  struct MountInfo {
    MountInfo() = default;
    MountInfo(MountInfo&& other) = default;
    MountInfo(const std::string& mount_root,
              base::TickClock* tick_clock,
              SmbCredential credential,
              std::unique_ptr<SambaInterface> samba_interface)
        : mount_root(mount_root),
          credential(std::move(credential)),
          samba_interface(std::move(samba_interface)) {
      cache = std::make_unique<MetadataCache>(
          tick_clock, base::TimeDelta::FromMicroseconds(
                          kMetadataCacheLifetimeMicroseconds));
    }

    MountInfo& operator=(MountInfo&& other) = default;

    std::string mount_root;
    SmbCredential credential;
    std::unique_ptr<SambaInterface> samba_interface;
    std::unique_ptr<MetadataCache> cache;

    DISALLOW_COPY_AND_ASSIGN(MountInfo);
  };

  // Returns true if |mount_root| exists as a value in |mounts_|. This method is
  // only used for DCHECK to ensure that mounts_ is in sync with
  // mounted_shares_.
  bool ExistsInMounts(const std::string& mount_root) const;

  // Returns true if |mount_id| exists as a value in |samba_interface_map_|.
  // This method is only used for DCHECK to ensure that |mounts_| is in sync
  // |samba_interface_map_|
  bool ExistsInSambaInterfaceMap(const int32_t mount_id) const;

  // Returns a new MountInfo.
  MountInfo CreateMountInfo(const std::string& mount_root,
                            SmbCredential credential,
                            std::unique_ptr<SambaInterface> samba_interface);

  // Adds |samba_interface_id| to |mount_id| mapping to |samba_interface_map_|.
  void AddSambaInterfaceIdToSambaInterfaceMap(int32_t mount_id);

  // Returns the SambaInterfaceId corresponding to the |mount_id|.
  SambaInterface::SambaInterfaceId GetSambaInterfaceIdForMountId(
      int32_t mount_id) const;

  // Removes |samba_interface_id| from |samba_interface_map_|.
  void DeleteSambaInterfaceIdFromSambaInterfaceMap(int32_t mount_id);

  // Maps MountId to MountInfo.
  IdMap<MountInfo> mounts_;

  // Maps SambaInterfaceId to MountId.
  std::unordered_map<SambaInterface::SambaInterfaceId, int32_t>
      samba_interface_map_;

  // Keeps track of share paths that have been mounted.
  std::unordered_set<std::string> mounted_share_paths_;

  std::unique_ptr<base::TickClock> tick_clock_;

  DISALLOW_COPY_AND_ASSIGN(MountTracker);
};

}  // namespace smbprovider

#endif  // SMBPROVIDER_MOUNT_TRACKER_H_
