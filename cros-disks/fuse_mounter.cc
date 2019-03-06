// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cros-disks/fuse_mounter.h"

#include <linux/capability.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <string>

#include <base/macros.h>
#include <base/bind.h>
#include <base/callback_helpers.h>
#include <base/files/file.h>
#include <base/logging.h>
#include <base/strings/stringprintf.h>
#include <base/strings/string_util.h>

#include "cros-disks/platform.h"
#include "cros-disks/sandboxed_process.h"

using std::string;

namespace cros_disks {

namespace {

const mode_t kSourcePathPermissions = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP;
const mode_t kTargetPathPermissions = S_IRWXU | S_IRWXG;

const char kFuseDeviceFile[] = "/dev/fuse";
const MountOptions::Flags kRequiredFuseMountFlags =
    MS_NODEV | MS_NOEXEC | MS_NOSUID;

}  // namespace

FUSEMounter::FUSEMounter(const string& source_path,
                         const string& target_path,
                         const string& filesystem_type,
                         const MountOptions& mount_options,
                         const Platform* platform,
                         const string& mount_program_path,
                         const string& mount_user,
                         const std::string& seccomp_policy,
                         const std::vector<std::string>& accessible_paths,
                         bool permit_network_access,
                         bool unprivileged_mount)
    : Mounter(source_path, target_path, filesystem_type, mount_options),
      platform_(platform),
      mount_program_path_(mount_program_path),
      mount_user_(mount_user),
      seccomp_policy_(seccomp_policy),
      accessible_paths_(accessible_paths),
      permit_network_access_(permit_network_access),
      unprivileged_mount_(unprivileged_mount) {}

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

  base::ScopedClosureRunner fuse_failure_unmounter;
  base::File fuse_file;
  if (unprivileged_mount_) {
    LOG(INFO) << "Using deprivileged FUSE with fd passing.";

    fuse_file = OpenFuseDeviceFile();
    if (!fuse_file.IsValid()) {
      return MOUNT_ERROR_INTERNAL;
    }

    if (!MountFuseDevice(fuse_file, mount_user_id, mount_group_id)) {
      return MOUNT_ERROR_INTERNAL;
    }

    // Unmount the FUSE filesystem if any later part fails.
    fuse_failure_unmounter.ReplaceClosure(base::Bind(
        [](const Platform* platform, const std::string& target_path) {
          if (!platform->Unmount(target_path, 0)) {
            LOG(ERROR) << "Failed to unmount " << target_path
                       << " on deprivileged fuse mount failure.";
          }
        },
        platform_, target_path()));
  }

  SandboxedProcess mount_process;
  mount_process.SetUserId(mount_user_id);
  mount_process.SetGroupId(mount_group_id);
  mount_process.SetNoNewPrivileges();
  // TODO(crbug.com/866377): Run FUSE fully deprivileged.
  // Currently SYS_ADMIN is needed to perform mount()/umount() calls from
  // libfuse.
  uint64_t capabilities = 0;
  if (!unprivileged_mount_) {
    capabilities |= CAP_TO_MASK(CAP_SYS_ADMIN);
  }
  mount_process.SetCapabilities(capabilities);

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

  // If a block device is being mounted, bind mount it into the sandbox.
  if (base::StartsWith(source_path(), "/dev/", base::CompareCase::SENSITIVE)) {
    if (!mount_process.BindMount(source_path(), source_path(), true)) {
      LOG(ERROR) << "Unable to bind mount device " << source_path();
      return MOUNT_ERROR_INTERNAL;
    }
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
  if (!mount_process.Mount("tmpfs", "/home", "tmpfs", "mode=0755,size=10M")) {
    LOG(ERROR) << "Can't mount /home";
    return MOUNT_ERROR_INTERNAL;
  }

  if (!unprivileged_mount_) {
    // Bind the FUSE device file.
    if (!mount_process.BindMount(kFuseDeviceFile, kFuseDeviceFile, true)) {
      LOG(ERROR) << "Unable to bind mount FUSE device file";
      return MOUNT_ERROR_INTERNAL;
    }

    // Mounts are exposed to the rest of the system through this shared mount.
    if (!mount_process.BindMount("/media", "/media", true)) {
      LOG(ERROR) << "Can't bind mount /media";
      return MOUNT_ERROR_INTERNAL;
    }
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
  if (unprivileged_mount_) {
    mount_process.AddArgument(
        base::StringPrintf("/dev/fd/%d", fuse_file.GetPlatformFile()));
  } else {
    mount_process.AddArgument(target_path());
  }

  if (!seccomp_policy_.empty()) {
    mount_process.LoadSeccompFilterPolicy(seccomp_policy_);
  }

  int return_code = mount_process.Run();
  if (return_code != 0) {
    LOG(WARNING) << "FUSE mount program failed with a return code "
                 << return_code;
    return MOUNT_ERROR_MOUNT_PROGRAM_FAILED;
  }
  // The |fuse_failure_unmounter| closure runner is used to unmount the FUSE
  // filesystem (for unprivileged mounts) if any part of starting the FUSE
  // helper process fails. At this point, the process has successfully started,
  // so release the closure runner to prevent the FUSE mount point from being
  // unmounted.
  ignore_result(fuse_failure_unmounter.Release());
  return MOUNT_ERROR_NONE;
}

base::File FUSEMounter::OpenFuseDeviceFile() const {
  base::File fuse_file(
      base::FilePath(kFuseDeviceFile),
      base::File::FLAG_OPEN | base::File::FLAG_READ | base::File::FLAG_WRITE);
  if (!fuse_file.IsValid()) {
    LOG(ERROR) << "Unable to open FUSE device file. Error: "
               << fuse_file.error_details() << " "
               << base::File::ErrorToString(fuse_file.error_details());
  }
  return fuse_file;
}

bool FUSEMounter::MountFuseDevice(const base::File& fuse_file,
                                  uid_t mount_user_id,
                                  gid_t mount_group_id) const {
  // Mount options for FUSE:
  // fd - File descriptor for /dev/fuse.
  // user_id/group_id - user/group for file access control. Essentially
  //     bypassed due to allow_other, but still required to be set.
  // allow_other - Allows users other than user_id/group_id to access files
  //     on the file system. By default, FUSE prevents any process other than
  //     ones running under user_id/group_id to access files, regardless of
  //     the file's permissions.
  // default_permissions - Enforce permission checking.
  // rootmode - Mode bits for the root inode.
  std::string fuse_mount_options = base::StringPrintf(
      "fd=%d,user_id=%u,group_id=%u,allow_other,default_permissions,"
      "rootmode=%o",
      fuse_file.GetPlatformFile(), mount_user_id, mount_group_id, S_IFDIR);

  // "nosymfollow" is a special mount option that's passed to the Chromium LSM
  // and not forwarded to the FUSE driver. If it's set, add it as a mount
  // option.
  if (mount_options().HasOption(MountOptions::kOptionNoSymFollow)) {
    fuse_mount_options.append(",");
    fuse_mount_options.append(MountOptions::kOptionNoSymFollow);
  }

  std::string fuse_type = "fuse";
  struct stat statbuf = {0};
  if (stat(source_path().c_str(), &statbuf) == 0 && S_ISBLK(statbuf.st_mode)) {
    LOG(INFO) << "Source file " << source_path() << " is a block device.";
    // TODO(crbug.com/931500): Determine and set blksize mount option. Default
    // is 512, which works everywhere, but is not necessarily optimal. Any
    // power-of-2 in the range [512, PAGE_SIZE] will work, but the optimal size
    // is the block/cluster size of the file system.
    fuse_type = "fuseblk";
  }

  return platform_->Mount(
      source_path(), target_path(), fuse_type,
      mount_options().ToMountFlagsAndData().first | kRequiredFuseMountFlags,
      fuse_mount_options);
}

}  // namespace cros_disks
