// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cros-disks/fuse_mounter.h"

#include <linux/capability.h>

#include <string>

#include <base/files/file_util.h>
#include <base/logging.h>

#include "cros-disks/platform.h"
#include "cros-disks/sandboxed_process.h"

using base::FilePath;
using std::string;

namespace {

// Process capabilities required by the FUSE mount process:
//   CAP_SYS_ADMIN for mounting/unmounting filesystem
//   CAP_SETUID/CAP_SETGID for setting uid/gid in non-privileged mounts
const uint64_t kMountProgramCapabilities =
    (1 << CAP_SYS_ADMIN) | (1 << CAP_SETUID) | (1 << CAP_SETGID);

const mode_t kSourcePathPermissions = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP;
const mode_t kTargetPathPermissions = S_IRWXU | S_IRWXG;

}  // namespace

namespace cros_disks {

FUSEMounter::FUSEMounter(const string& source_path,
                         const string& target_path,
                         const string& filesystem_type,
                         const MountOptions& mount_options,
                         const Platform* platform,
                         const string& mount_program_path,
                         const string& mount_user)
    : Mounter(source_path, target_path, filesystem_type, mount_options),
      platform_(platform),
      mount_program_path_(mount_program_path),
      mount_user_(mount_user) {
}

MountErrorType FUSEMounter::MountImpl() {
  if (!base::PathExists(FilePath(mount_program_path_))) {
    LOG(ERROR) << "Failed to find the FUSE mount program";
    return MOUNT_ERROR_MOUNT_PROGRAM_NOT_FOUND;
  }

  SandboxedProcess mount_process;
  mount_process.SetCapabilities(kMountProgramCapabilities);

  uid_t mount_user_id;
  gid_t mount_group_id;
  // To perform a non-privileged mount via the FUSE mount program, change the
  // group of the source and target path to the group of the non-privileged
  // user, but keep the user of the source and target path unchanged. Also set
  // appropriate group permissions on the source and target path.
  if (!platform_->GetUserAndGroupId(mount_user_,
                                    &mount_user_id, &mount_group_id) ||
      !platform_->SetOwnership(source_path(), getuid(), mount_group_id) ||
      !platform_->SetPermissions(source_path(), kSourcePathPermissions) ||
      !platform_->SetOwnership(target_path(), getuid(), mount_group_id) ||
      !platform_->SetPermissions(target_path(), kTargetPathPermissions)) {
    return MOUNT_ERROR_INTERNAL;
  }
  mount_process.SetUserId(mount_user_id);
  mount_process.SetGroupId(mount_group_id);
  mount_process.SetNoNewPrivileges();

  // The FUSE mount program is put under a new mount namespace, so mounts
  // inside that namespace don't normally propagate out except when a mount is
  // created under /media, which is marked as a shared mount (by
  // chromeos_startup). This prevents the FUSE mount program from remounting an
  // existing mount point outside /media.
  //
  // TODO(benchan): It's fragile to assume chromeos_startup makes /media a
  // shared mount. cros-disks should verify that and make /media a shared mount
  // when necessary.
  mount_process.NewMountNamespace();
  // Prevent minjail from turning /media private again.
  //
  // TODO(benchan): Revisit this once minijail provides a finer control over
  // what should be remounted private and what can remain shared (b:62056108).
  mount_process.SkipRemountPrivate();

  // TODO(benchan): Re-enable cgroup namespace when either Chrome OS
  // kernel 3.8 supports it or no more supported devices use kernel
  // 3.8.
  // mount_process.NewCgroupNamespace();

  mount_process.NewIpcNamespace();
  mount_process.NewNetworkNamespace();

  mount_process.AddArgument(mount_program_path_);
  string options_string = mount_options().ToString();
  if (!options_string.empty()) {
    mount_process.AddArgument("-o");
    mount_process.AddArgument(options_string);
  }
  mount_process.AddArgument(source_path());
  mount_process.AddArgument(target_path());

  int return_code = mount_process.Run();
  if (return_code != 0) {
    LOG(WARNING) << "FUSE mount program failed with a return code "
                 << return_code;
    return MOUNT_ERROR_MOUNT_PROGRAM_FAILED;
  }
  return MOUNT_ERROR_NONE;
}

}  // namespace cros_disks
