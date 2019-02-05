// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CROS_DISKS_FUSE_MOUNTER_H_
#define CROS_DISKS_FUSE_MOUNTER_H_

#include <sys/types.h>

#include <string>
#include <vector>

#include <base/files/file.h>

#include "cros-disks/mounter.h"

namespace cros_disks {

class Platform;

// A class for mounting a device file using a FUSE mount program.
class FUSEMounter : public Mounter {
 public:
  FUSEMounter(const std::string& source_path,
              const std::string& target_path,
              const std::string& filesystem_type,
              const MountOptions& mount_options,
              const Platform* platform,
              const std::string& mount_program_path,
              const std::string& mount_user,
              const std::string& seccomp_policy,
              const std::vector<std::string>& accessible_paths,
              bool permit_network_access,
              bool unprivileged_mount = false);

 protected:
  // Mounts a device file using the FUSE mount program at |mount_program_path_|.
  MountErrorType MountImpl() override;

  // An object that provides platform service.
  const Platform* const platform_;

  // Path of the FUSE mount program.
  const std::string mount_program_path_;

  // User to run the FUSE mount program as.
  const std::string mount_user_;

  // If not empty the path to BPF seccomp filter policy.
  const std::string seccomp_policy_;

  // Directories the FUSE module should be able to access (beyond basic
  // /proc, /dev, etc).
  const std::vector<std::string> accessible_paths_;

  // Whether to leave network access to the mount program.
  const bool permit_network_access_;

  // Whether to run the fuse program deprivileged.
  // TODO(crbug.com/866377): Remove when all fuse programs can run without
  // privileges.
  const bool unprivileged_mount_;

 private:
  // Returns an opened FUSE device file.
  base::File OpenFuseDeviceFile() const;

  // Mount a FUSE device for unprivileged FUSE mounts.
  bool MountFuseDevice(const base::File& fuse_file,
                       uid_t mount_user_id,
                       gid_t mount_group_id) const;
};

}  // namespace cros_disks

#endif  // CROS_DISKS_FUSE_MOUNTER_H_
