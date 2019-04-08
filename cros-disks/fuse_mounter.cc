// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cros-disks/fuse_mounter.h"

#include <linux/capability.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <memory>
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

MountErrorType ConfigureCommonSandbox(SandboxedProcess* sandbox,
                                      const Platform* platform,
                                      bool network_ns,
                                      const base::FilePath& seccomp,
                                      bool unprivileged) {
  // TODO(crbug.com/866377): Run FUSE fully deprivileged.
  // Currently SYS_ADMIN is needed to perform mount()/umount() calls from
  // libfuse.
  uint64_t capabilities = unprivileged ? 0 : CAP_TO_MASK(CAP_SYS_ADMIN);
  sandbox->SetCapabilities(capabilities);
  sandbox->SetNoNewPrivileges();

  // The FUSE mount program is put under a new mount namespace, so mounts
  // inside that namespace don't normally propagate out except when a mount is
  // created under /media, which is marked as a shared mount (by
  // chromeos_startup). This prevents the FUSE mount program from remounting an
  // existing mount point outside /media.
  //
  // TODO(benchan): It's fragile to assume chromeos_startup makes /media a
  // shared mount. cros-disks should verify that and make /media a shared mount
  // when necessary.
  sandbox->NewMountNamespace();

  // Prevent minjail from turning /media private again.
  //
  // TODO(benchan): Revisit this once minijail provides a finer control over
  // what should be remounted private and what can remain shared (b:62056108).
  sandbox->SkipRemountPrivate();

  // TODO(benchan): Re-enable cgroup namespace when either Chrome OS
  // kernel 3.8 supports it or no more supported devices use kernel
  // 3.8.
  // mount_process.NewCgroupNamespace();

  sandbox->NewIpcNamespace();

  if (network_ns) {
    sandbox->NewNetworkNamespace();
  }

  if (!seccomp.empty()) {
    if (!platform->PathExists(seccomp.value())) {
      LOG(ERROR) << "Seccomp policy '" << seccomp.value() << "' is missing";
      return MountErrorType::MOUNT_ERROR_INTERNAL;
    }
    sandbox->LoadSeccompFilterPolicy(seccomp.value());
  }

  // Prepare mounts for pivot_root.
  if (!sandbox->SetUpMinimalMounts()) {
    LOG(ERROR) << "Can't set up minijail mounts";
    return MountErrorType::MOUNT_ERROR_INTERNAL;
  }

  if (!unprivileged) {
    // Bind the FUSE device file.
    if (!sandbox->BindMount(kFuseDeviceFile, kFuseDeviceFile, true, false)) {
      LOG(ERROR) << "Unable to bind FUSE device file";
      return MountErrorType::MOUNT_ERROR_INTERNAL;
    }

    // Mounts are exposed to the rest of the system through this shared mount.
    if (!sandbox->BindMount("/media", "/media", true, false)) {
      LOG(ERROR) << "Can't bind /media";
      return MountErrorType::MOUNT_ERROR_INTERNAL;
    }
  }

  // Data dirs if any are mounted inside /run/fuse.
  if (!sandbox->Mount("tmpfs", "/run", "tmpfs", "mode=0755,size=10M")) {
    LOG(ERROR) << "Can't mount /run";
    return MountErrorType::MOUNT_ERROR_INTERNAL;
  }
  if (!sandbox->BindMount("/run/fuse", "/run/fuse", false, false)) {
    LOG(ERROR) << "Can't bind /run/fuse";
    return MountErrorType::MOUNT_ERROR_INTERNAL;
  }

  if (!network_ns) {
    // Network DNS configs are in /run/shill.
    if (!sandbox->BindMount("/run/shill", "/run/shill", false, false)) {
      LOG(ERROR) << "Can't bind /run/shill";
      return MountErrorType::MOUNT_ERROR_INTERNAL;
    }
    // Hardcoded hosts are mounted into /etc/hosts.d when Crostini is enabled.
    if (platform->PathExists("/etc/hosts.d") &&
        !sandbox->BindMount("/etc/hosts.d", "/etc/hosts.d", false, false)) {
      LOG(ERROR) << "Can't bind /etc/hosts.d";
      return MountErrorType::MOUNT_ERROR_INTERNAL;
    }
  }
  if (!sandbox->EnterPivotRoot()) {
    LOG(ERROR) << "Can't pivot root";
    return MountErrorType::MOUNT_ERROR_INTERNAL;
  }

  return MountErrorType::MOUNT_ERROR_NONE;
}

MountErrorType MountFuseDevice(const Platform* platform,
                               const std::string& source,
                               const base::FilePath& target,
                               const base::File& fuse_file,
                               uid_t mount_user_id,
                               gid_t mount_group_id,
                               const MountOptions& options) {
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
  if (options.HasOption(MountOptions::kOptionNoSymFollow)) {
    fuse_mount_options.append(",");
    fuse_mount_options.append(MountOptions::kOptionNoSymFollow);
  }

  std::string fuse_type = "fuse";
  struct stat statbuf = {0};
  if (stat(source.c_str(), &statbuf) == 0 && S_ISBLK(statbuf.st_mode)) {
    LOG(INFO) << "Source file " << source << " is a block device.";
    // TODO(crbug.com/931500): Determine and set blksize mount option. Default
    // is 512, which works everywhere, but is not necessarily optimal. Any
    // power-of-2 in the range [512, PAGE_SIZE] will work, but the optimal size
    // is the block/cluster size of the file system.
    fuse_type = "fuseblk";
  }

  auto flags = options.ToMountFlagsAndData().first;

  return platform->Mount(source, target.value(), fuse_type,
                         flags | kRequiredFuseMountFlags, fuse_mount_options);
}

}  // namespace

FUSEMounter::FUSEMounter(const string& source_path,
                         const string& target_path,
                         const string& filesystem_type,
                         const MountOptions& mount_options,
                         const Platform* platform,
                         const string& mount_program_path,
                         const string& mount_user,
                         const std::string& seccomp_policy,
                         const std::vector<BindPath>& accessible_paths,
                         bool permit_network_access,
                         bool unprivileged_mount,
                         const std::string& mount_group)
    : MounterCompat(filesystem_type,
                    source_path,
                    base::FilePath(target_path),
                    mount_options),
      platform_(platform),
      mount_program_path_(mount_program_path),
      mount_user_(mount_user),
      mount_group_(mount_group),
      seccomp_policy_(seccomp_policy),
      accessible_paths_(accessible_paths),
      permit_network_access_(permit_network_access),
      unprivileged_mount_(unprivileged_mount) {}

MountErrorType FUSEMounter::MountImpl() const {
  auto mount_process = CreateSandboxedProcess();
  MountErrorType error = ConfigureCommonSandbox(
      mount_process.get(), platform_, !permit_network_access_,
      base::FilePath(seccomp_policy_), unprivileged_mount_);
  if (error != MountErrorType::MOUNT_ERROR_NONE) {
    return error;
  }

  uid_t mount_user_id;
  gid_t mount_group_id;
  if (!platform_->GetUserAndGroupId(mount_user_, &mount_user_id,
                                    &mount_group_id)) {
    LOG(ERROR) << "Can't resolve user '" << mount_user_ << "'";
    return MountErrorType::MOUNT_ERROR_INTERNAL;
  }
  if (!mount_group_.empty() &&
      !platform_->GetGroupId(mount_group_, &mount_group_id)) {
    return MOUNT_ERROR_INTERNAL;
  }

  mount_process->SetUserId(mount_user_id);
  mount_process->SetGroupId(mount_group_id);

  if (!platform_->PathExists(mount_program_path_)) {
    LOG(ERROR) << "Mount program '" << mount_program_path_ << "' not found.";
    return MountErrorType::MOUNT_ERROR_MOUNT_PROGRAM_NOT_FOUND;
  }
  mount_process->AddArgument(mount_program_path_);

  base::ScopedClosureRunner fuse_failure_unmounter;
  base::File fuse_file;
  if (unprivileged_mount_) {
    LOG(INFO) << "Using deprivileged FUSE with fd passing.";

    fuse_file = base::File(
        base::FilePath(kFuseDeviceFile),
        base::File::FLAG_OPEN | base::File::FLAG_READ | base::File::FLAG_WRITE);
    if (!fuse_file.IsValid()) {
      LOG(ERROR) << "Unable to open FUSE device file. Error: "
                 << fuse_file.error_details() << " "
                 << base::File::ErrorToString(fuse_file.error_details());
      return MountErrorType::MOUNT_ERROR_INTERNAL;
    }

    error = MountFuseDevice(platform_, source(), base::FilePath(target_path()),
                            fuse_file, mount_user_id, mount_group_id,
                            mount_options());
    if (error != MountErrorType::MOUNT_ERROR_NONE) {
      LOG(ERROR) << "Can't perform unprivileged FUSE mount";
      return error;
    }

    // Unmount the FUSE filesystem if any later part fails.
    fuse_failure_unmounter.ReplaceClosure(base::Bind(
        [](const Platform* platform, const std::string& target_path) {
          if (!platform->Unmount(target_path, 0)) {
            LOG(ERROR) << "Failed to unmount " << target_path
                       << " on deprivileged fuse mount failure.";
          }
        },
        platform_, target_path().value()));
  } else {
    // To perform a non-root mount via the FUSE mount program, change the
    // group of the source and target path to the group of the non-privileged
    // user, but keep the user of the source and target path unchanged. Also set
    // appropriate group permissions on the source and target path.
    if (!platform_->SetOwnership(target_path().value(), getuid(),
                                 mount_group_id) ||
        !platform_->SetPermissions(target_path().value(),
                                   kTargetPathPermissions)) {
      LOG(ERROR) << "Can't set up permissions on the mount point";
      return MountErrorType::MOUNT_ERROR_INSUFFICIENT_PERMISSIONS;
    }
  }

  // Source might be an URI. Only try to re-own source if it looks like
  // an existing path.
  if (!source().empty() && platform_->PathExists(source())) {
    if (!platform_->SetOwnership(source(), getuid(), mount_group_id) ||
        !platform_->SetPermissions(source(), kSourcePathPermissions)) {
      LOG(ERROR) << "Can't set up permissions on the source";
      return MountErrorType::MOUNT_ERROR_INSUFFICIENT_PERMISSIONS;
    }
  }

  // If a block device is being mounted, bind mount it into the sandbox.
  if (base::StartsWith(source(), "/dev/", base::CompareCase::SENSITIVE)) {
    if (!mount_process->BindMount(source(), source(), true, false)) {
      LOG(ERROR) << "Unable to bind mount device " << source();
      return MountErrorType::MOUNT_ERROR_INVALID_ARGUMENT;
    }
  }

  // TODO(crbug.com/933018): Remove when DriveFS helper is refactored.
  if (!mount_process->Mount("tmpfs", "/home", "tmpfs", "mode=0755,size=10M")) {
    LOG(ERROR) << "Can't mount /home";
    return MountErrorType::MOUNT_ERROR_INTERNAL;
  }

  // This is for additional data dirs.
  for (const auto& path : accessible_paths_) {
    if (!mount_process->BindMount(path.path, path.path, path.writable,
                                  path.recursive)) {
      LOG(ERROR) << "Can't bind " << path.path;
      return MountErrorType::MOUNT_ERROR_INVALID_ARGUMENT;
    }
  }

  string options_string = mount_options().ToString();
  if (!options_string.empty()) {
    mount_process->AddArgument("-o");
    mount_process->AddArgument(options_string);
  }
  if (!source().empty()) {
    mount_process->AddArgument(source());
  }
  if (unprivileged_mount_) {
    mount_process->AddArgument(
        base::StringPrintf("/dev/fd/%d", fuse_file.GetPlatformFile()));
  } else {
    mount_process->AddArgument(target_path().value());
  }

  int return_code = mount_process->Run();
  if (return_code != 0) {
    LOG(WARNING) << "FUSE mount program failed with a return code "
                 << return_code;
    return MountErrorType::MOUNT_ERROR_MOUNT_PROGRAM_FAILED;
  }
  // The |fuse_failure_unmounter| closure runner is used to unmount the FUSE
  // filesystem (for unprivileged mounts) if any part of starting the FUSE
  // helper process fails. At this point, the process has successfully started,
  // so release the closure runner to prevent the FUSE mount point from being
  // unmounted.
  ignore_result(fuse_failure_unmounter.Release());
  return MountErrorType::MOUNT_ERROR_NONE;
}

std::unique_ptr<SandboxedProcess> FUSEMounter::CreateSandboxedProcess() const {
  return std::make_unique<SandboxedProcess>();
}

}  // namespace cros_disks
