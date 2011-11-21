// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cros-disks/ntfs-mounter.h"

#include <linux/capability.h>

#include <string>

#include <base/file_util.h>
#include <base/logging.h>

#include "cros-disks/platform.h"
#include "cros-disks/sandboxed-process.h"

using std::string;

namespace {

// Expected location of the ntfs-3g executable.
const char kMountProgramPath[] = "/bin/ntfs-3g";

const char kMountUser[] = "ntfs-3g";

// Process capabilities required by the ntfs-3g process:
//   CAP_SYS_ADMIN for mounting/unmounting filesystem
//   CAP_SETUID/CAP_SETGID for setting uid/gid in non-privileged mounts
const uint64_t kMountProgramCapabilities =
    (1 << CAP_SYS_ADMIN) | (1 << CAP_SETUID) | (1 << CAP_SETGID);

const mode_t kSourcePathPermissions = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP;
const mode_t kTargetPathPermissions = S_IRWXU | S_IRWXG;

}  // namespace

namespace cros_disks {

const char NTFSMounter::kMounterType[] = "ntfs";

NTFSMounter::NTFSMounter(const string& source_path,
                         const string& target_path,
                         const string& filesystem_type,
                         const MountOptions& mount_options,
                         Platform* platform)
    : Mounter(source_path, target_path, filesystem_type, mount_options),
      platform_(platform) {
}

MountErrorType NTFSMounter::MountImpl() {
  if (!file_util::PathExists(FilePath(kMountProgramPath))) {
    LOG(ERROR) << "Failed to find the ntfs-3g mount program";
    return MOUNT_ERROR_MOUNT_PROGRAM_NOT_FOUND;
  }

  SandboxedProcess mount_process;
  mount_process.SetCapabilities(kMountProgramCapabilities);

  // Temporarily work around the fact that the ntfs-3g executable is not
  // set to setuid-root, so check the permissions of ntfs-3g before doing
  // a non-privileged mount and fall back to a privileged mount if necessary.
  // TODO(benchan): Remove this workaround.
  uid_t program_user_id;
  mode_t program_mode;
  bool non_privileged_mount =
      platform_->GetOwnership(kMountProgramPath, &program_user_id, NULL) &&
      platform_->GetPermissions(kMountProgramPath, &program_mode) &&
      (program_user_id == 0) && ((program_mode & S_ISUID) != 0);

  if (non_privileged_mount) {
    uid_t mount_user_id;
    gid_t mount_group_id;
    // To perform a non-privileged mount via ntfs-3g, change the group of
    // the source and target path to the group of the non-privileged user,
    // but keep the user of the source and target path unchanged. Also set
    // appropriate group permissions on the source and target path.
    if (!platform_->GetUserAndGroupId(kMountUser,
                                      &mount_user_id, &mount_group_id) ||
        !platform_->SetOwnership(source_path(), getuid(), mount_group_id) ||
        !platform_->SetPermissions(source_path(), kSourcePathPermissions) ||
        !platform_->SetOwnership(target_path(), getuid(), mount_group_id) ||
        !platform_->SetPermissions(target_path(), kTargetPathPermissions)) {
      return MOUNT_ERROR_INTERNAL;
    }
    mount_process.SetUserId(mount_user_id);
    mount_process.SetGroupId(mount_group_id);
  }

  mount_process.AddArgument(kMountProgramPath);
  string options_string = mount_options().ToString();
  if (!options_string.empty()) {
    mount_process.AddArgument("-o");
    mount_process.AddArgument(options_string);
  }
  mount_process.AddArgument(source_path());
  mount_process.AddArgument(target_path());

  int return_code = mount_process.Run();
  if (return_code != 0) {
    LOG(WARNING) << "NTFS-3G mount program failed with a return code "
                 << return_code;
    return MOUNT_ERROR_MOUNT_PROGRAM_FAILED;
  }
  return MOUNT_ERROR_NONE;
}

}  // namespace cros_disks
