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

#include "smbprovider/constants.h"
#include "smbprovider/id_map.h"
#include "smbprovider/metadata_cache.h"
#include "smbprovider/samba_interface.h"
#include "smbprovider/smb_credential.h"

namespace base {
class TickClock;
};

namespace smbprovider {

class MountTracker {
 public:
  MountTracker();
  ~MountTracker();

  // Returns true if |mount_id| is already mounted.
  bool IsAlreadyMounted(int32_t mount_id) const;

  // Returns true if |mount_root| is already mounted.
  bool IsAlreadyMounted(const std::string& mount_root) const;

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

  // Returns true if |mount_root| is already mounted.
  bool ExistsInMountedSharePaths(const std::string& mount_root) const;

  // Maps MountId to MountInfo.
  IdMap<MountInfo> mounts_;

  // Maps SambaInterfaceId to MountId.
  std::unordered_map<SambaInterface::SambaInterfaceId, int32_t>
      samba_interface_map_;

  // Keeps track of share paths that have been mounted.
  std::unordered_set<std::string> mounted_share_paths_;

  DISALLOW_COPY_AND_ASSIGN(MountTracker);
};

}  // namespace smbprovider

#endif  // SMBPROVIDER_MOUNT_TRACKER_H_
