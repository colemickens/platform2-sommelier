// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SMBPROVIDER_MOUNT_TRACKER_H_
#define SMBPROVIDER_MOUNT_TRACKER_H_

#include <memory>
#include <string>
#include <utility>

#include <base/callback.h>
#include <base/macros.h>

#include "smbprovider/constants.h"
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

  DISALLOW_COPY_AND_ASSIGN(MountTracker);
};

}  // namespace smbprovider

#endif  // SMBPROVIDER_MOUNT_TRACKER_H_
