// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cros-disks/fuse_mounter.h"

#include <linux/capability.h>

#include <string>

#include <base/logging.h>
#include <base/strings/string_util.h>

#include "cros-disks/platform.h"
#include "cros-disks/sandboxed_process.h"

using std::string;

namespace {

const mode_t kSourcePathPermissions = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP;
const mode_t kTargetPathPermissions = S_IRWXU | S_IRWXG;

const char kFuseDeviceFile[] = "/dev/fuse";

}  // namespace

namespace cros_disks {

FUSEMounter::FUSEMounter(const string& source_path,
                         const string& target_path,
                         const string& filesystem_type,
                         const MountOptions& mount_options,
                         const Platform* platform,
                         const string& mount_program_path,
                         const string& mount_user,
                         const std::string& seccomp_policy,
                         const std::vector<std::string>& accessible_paths,
                         bool permit_network_access)
    : Mounter(source_path, target_path, filesystem_type, mount_options),
      platform_(platform),
      mount_program_path_(mount_program_path),
      mount_user_(mount_user),
      seccomp_policy_(seccomp_policy),
      accessible_paths_(accessible_paths),
      permit_network_access_(permit_network_access) {}

MountErrorType FUSEMounter::MountImpl() {
  if (!platform_->PathExists(mount_program_path_)) {
    LOG(ERROR) << "Failed to find the FUSE mount program";
    return MOUNT_ERROR_MOUNT_PROGRAM_NOT_FOUND;
  }

  uid_t mount_user_id;
  gid_t mount_group_id;
  if (!platform_->GetUserAndGroupId(mount_user_, &mount_user_id,
                                    &mount_group_id)) {
    return MOUNT_ERROR_INTERNAL;
  }

  // To perform a non-privileged mount via the FUSE mount program, change the
  // group of the source and target path to the group of the non-privileged
  // user, but keep the user of the source and target path unchanged. Also set
  // appropriate group permissions on the source and target path.
  if (!platform_->SetOwnership(target_path(), getuid(), mount_group_id) ||
      !platform_->SetPermissions(target_path(), kTargetPathPermissions)) {
    return MOUNT_ERROR_INTERNAL;
  }

  // Source might be an URI. Only try to re-own source if it looks like
  // an existing path.
  if (platform_->PathExists(source_path())) {
    if (!platform_->SetOwnership(source_path(), getuid(), mount_group_id) ||
        !platform_->SetPermissions(source_path(), kSourcePathPermissions)) {
      return MOUNT_ERROR_INTERNAL;
    }
  }

  SandboxedProcess mount_process;
  mount_process.SetUserId(mount_user_id);
  mount_process.SetGroupId(mount_group_id);
  mount_process.SetNoNewPrivileges();
  // TODO(crbug.com/866377): Run FUSE fully deprivileged.
  // Currently SYS_ADMIN is needed to perform mount()/umount() calls from
  // libfuse and SYS_CHROOT is needed to call pivot_root() from minijail.
  mount_process.SetCapabilities(CAP_TO_MASK(CAP_SYS_ADMIN) |
                                CAP_TO_MASK(CAP_SYS_CHROOT));

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
  if (!mount_process.SetUpMinimalMounts()) {
    LOG(ERROR) << "Can't set up minijail mounts";
    return MOUNT_ERROR_INTERNAL;
  }

  // Bind the FUSE device file.
  if (!mount_process.BindMount(kFuseDeviceFile, kFuseDeviceFile, true)) {
    LOG(ERROR) << "Unable to bind FUSE device file";
    return MOUNT_ERROR_INTERNAL;
  }

  // If a block device is being mounted, bind mount it into the sandbox.
  if (base::StartsWith(source_path(), "/dev/", base::CompareCase::SENSITIVE)) {
    if (!mount_process.BindMount(source_path(), source_path(), true)) {
      LOG(ERROR) << "Unable to bind mount device " << source_path();
      return MOUNT_ERROR_INTERNAL;
    }
  }

  // Mounts are exposed to the rest of the system through this shared mount.
  if (!mount_process.BindMount("/media", "/media", true)) {
    LOG(ERROR) << "Can't bind /media";
    return MOUNT_ERROR_INTERNAL;
  }

  // Data dirs if any are mounted inside /run/fuse.
  if (!mount_process.Mount("tmpfs", "/run", "tmpfs", "mode=0755,size=10M")) {
    LOG(ERROR) << "Can't mount /run";
    return MOUNT_ERROR_INTERNAL;
  }
  if (!mount_process.BindMount("/run/fuse", "/run/fuse", false)) {
    LOG(ERROR) << "Can't bind /run/fuse";
    return MOUNT_ERROR_INTERNAL;
  }

  // This is for additional data dirs.
  for (const auto& path : accessible_paths_) {
    if (!mount_process.BindMount(path, path, true)) {
      LOG(ERROR) << "Can't bind " << path;
      return MOUNT_ERROR_INVALID_ARGUMENT;
    }
  }

  // Prevent minjail from turning /media private again.
  //
  // TODO(benchan): Revisit this once minijail provides a finer control over
  // what should be remounted private and what can remain shared (b:62056108).
  mount_process.SkipRemountPrivate();

  if (!mount_process.EnterPivotRoot()) {
    LOG(ERROR) << "Can't pivot root";
    return MOUNT_ERROR_INTERNAL;
  }

  // TODO(benchan): Re-enable cgroup namespace when either Chrome OS
  // kernel 3.8 supports it or no more supported devices use kernel
  // 3.8.
  // mount_process.NewCgroupNamespace();

  mount_process.NewIpcNamespace();
  if (!permit_network_access_) {
    mount_process.NewNetworkNamespace();
  } else {
    // Network DNS configs are in /run/shill.
    if (!mount_process.BindMount("/run/shill", "/run/shill", false)) {
      LOG(ERROR) << "Can't bind /run/shill";
      return MOUNT_ERROR_INTERNAL;
    }
    // Hardcoded hosts are mounted into /etc/hosts.d when Crostini is enabled.
    if (platform_->PathExists("/etc/hosts.d") &&
        !mount_process.BindMount("/etc/hosts.d", "/etc/hosts.d", false)) {
      LOG(ERROR) << "Can't bind /etc/hosts.d";
      return MOUNT_ERROR_INTERNAL;
    }
  }

  mount_process.AddArgument(mount_program_path_);
  string options_string = mount_options().ToString();
  if (!options_string.empty()) {
    mount_process.AddArgument("-o");
    mount_process.AddArgument(options_string);
  }
  if (!source_path().empty()) {
    mount_process.AddArgument(source_path());
  }
  mount_process.AddArgument(target_path());

  if (!seccomp_policy_.empty()) {
    mount_process.LoadSeccompFilterPolicy(seccomp_policy_);
  }

  int return_code = mount_process.Run();
  if (return_code != 0) {
    LOG(WARNING) << "FUSE mount program failed with a return code "
                 << return_code;
    return MOUNT_ERROR_MOUNT_PROGRAM_FAILED;
  }
  return MOUNT_ERROR_NONE;
}

}  // namespace cros_disks
