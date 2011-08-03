// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cros-disks/ntfs-mounter.h"

#include <string>

#include <base/file_util.h>
#include <base/logging.h>
#include <chromeos/process.h>

#include "cros-disks/platform.h"

using std::string;

namespace cros_disks {

// Expected location of the ntfs-3g executable.
static const char kMountProgramPath[] = "/bin/ntfs-3g";

static const mode_t kSourcePathPermissions =
    S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP;

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
    return kMountErrorMountProgramNotFound;
  }

  chromeos::ProcessImpl mount_process;

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
    uid_t mount_user_id = platform_->mount_user_id();
    gid_t mount_group_id = platform_->mount_group_id();
    // To perform a non-privileged mount via ntfs-3g, change the group of
    // the source path to the group of the non-privileged user, but keep
    // the user of the source path unchanged.
    if (!platform_->SetOwnership(source_path(), getuid(), mount_group_id) ||
        !platform_->SetPermissions(source_path(), kSourcePathPermissions)) {
      return kMountErrorInternal;
    }
    mount_process.SetUid(mount_user_id);
    mount_process.SetGid(mount_group_id);
  }

  mount_process.AddArg(kMountProgramPath);
  string options_string = mount_options().ToString();
  if (!options_string.empty()) {
    mount_process.AddArg("-o");
    mount_process.AddArg(options_string);
  }
  mount_process.AddArg(source_path());
  mount_process.AddArg(target_path());

  int return_code = mount_process.Run();
  if (return_code != 0) {
    LOG(WARNING) << "NTFS-3G mount program failed with a return code "
                 << return_code;
    return kMountErrorMountProgramFailed;
  }
  return kMountErrorNone;
}

}  // namespace cros_disks
