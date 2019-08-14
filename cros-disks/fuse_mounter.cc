// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cros-disks/fuse_mounter.h"

#include <fcntl.h>
#include <linux/capability.h>
#include <linux/fs.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <memory>
#include <string>
#include <utility>

#include <base/macros.h>
#include <base/bind.h>
#include <base/callback_helpers.h>
#include <base/files/file.h>
#include <base/logging.h>
#include <base/strings/stringprintf.h>
#include <base/strings/string_util.h>
#include <brillo/process_reaper.h>

#include "cros-disks/error_logger.h"
#include "cros-disks/platform.h"
#include "cros-disks/quote.h"
#include "cros-disks/sandboxed_process.h"

namespace cros_disks {

namespace {

const mode_t kSourcePathPermissions = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP;

const char kFuseDeviceFile[] = "/dev/fuse";
const MountOptions::Flags kRequiredFuseMountFlags =
    MS_NODEV | MS_NOEXEC | MS_NOSUID;

void CleanUpCallback(base::OnceClosure cleanup,
                     const base::FilePath& mount_path,
                     const siginfo_t& info) {
  CHECK_EQ(SIGCHLD, info.si_signo);
  if (info.si_code != CLD_EXITED || info.si_status != 0) {
    LOG(WARNING) << "FUSE daemon for '" << mount_path.value()
                 << "' exited with code " << info.si_code << " and status "
                 << info.si_status;
  } else {
    LOG(INFO) << "FUSE daemon for '" << mount_path.value()
              << "' exited normally";
  }
  std::move(cleanup).Run();
}

MountErrorType ConfigureCommonSandbox(SandboxedProcess* sandbox,
                                      const Platform* platform,
                                      bool network_ns,
                                      const base::FilePath& seccomp) {
  sandbox->SetCapabilities(0);
  sandbox->SetNoNewPrivileges();

  // The FUSE mount program is put under a new mount namespace, so mounts
  // inside that namespace don't normally propagate.
  sandbox->NewMountNamespace();

  // TODO(crbug.com/707327): Remove this when we get rid of AVFS.
  sandbox->SkipRemountPrivate();

  // TODO(benchan): Re-enable cgroup namespace when either Chrome OS
  // kernel 3.8 supports it or no more supported devices use kernel
  // 3.8.
  // mount_process.NewCgroupNamespace();

  sandbox->NewIpcNamespace();

  sandbox->NewPidNamespace();

  if (network_ns) {
    sandbox->NewNetworkNamespace();
  }

  if (!seccomp.empty()) {
    if (!platform->PathExists(seccomp.value())) {
      LOG(ERROR) << "Seccomp policy '" << seccomp.value() << "' is missing";
      return MOUNT_ERROR_INTERNAL;
    }
    sandbox->LoadSeccompFilterPolicy(seccomp.value());
  }

  // Prepare mounts for pivot_root.
  if (!sandbox->SetUpMinimalMounts()) {
    LOG(ERROR) << "Can't set up minijail mounts";
    return MOUNT_ERROR_INTERNAL;
  }

  // Data dirs if any are mounted inside /run/fuse.
  if (!sandbox->Mount("tmpfs", "/run", "tmpfs", "mode=0755,size=10M")) {
    LOG(ERROR) << "Can't mount /run";
    return MOUNT_ERROR_INTERNAL;
  }
  if (!sandbox->BindMount("/run/fuse", "/run/fuse", false, false)) {
    LOG(ERROR) << "Can't bind /run/fuse";
    return MOUNT_ERROR_INTERNAL;
  }

  if (!network_ns) {
    // Network DNS configs are in /run/shill.
    if (!sandbox->BindMount("/run/shill", "/run/shill", false, false)) {
      LOG(ERROR) << "Can't bind /run/shill";
      return MOUNT_ERROR_INTERNAL;
    }
    // Hardcoded hosts are mounted into /etc/hosts.d when Crostini is enabled.
    if (platform->PathExists("/etc/hosts.d") &&
        !sandbox->BindMount("/etc/hosts.d", "/etc/hosts.d", false, false)) {
      LOG(ERROR) << "Can't bind /etc/hosts.d";
      return MOUNT_ERROR_INTERNAL;
    }
  }
  if (!sandbox->EnterPivotRoot()) {
    LOG(ERROR) << "Can't pivot root";
    return MOUNT_ERROR_INTERNAL;
  }

  return MOUNT_ERROR_NONE;
}

bool GetPhysicalBlockSize(const std::string& source, int* size) {
  base::ScopedFD fd(open(source.c_str(), O_RDONLY | O_CLOEXEC));

  *size = 0;
  if (!fd.is_valid()) {
    PLOG(WARNING) << "Couldn't open " << source;
    return false;
  }

  if (ioctl(fd.get(), BLKPBSZGET, size) < 0) {
    PLOG(WARNING) << "Failed to get block size for" << source;
    return false;
  }

  return true;
}

MountErrorType MountFuseDevice(const Platform* platform,
                               const std::string& source,
                               const std::string& filesystem_type,
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
    int blksize = 0;

    // TODO(crbug.com/931500): It's possible that specifying a block size equal
    // to the file system cluster size (which might be larger than the physical
    // block size) might be more efficient. Data would be needed to see what
    // kind of performance benefit, if any, could be gained. At the very least,
    // specify the block size of the underlying device. Without this, UFS cards
    // with 4k sector size will fail to mount.
    if (GetPhysicalBlockSize(source, &blksize) && blksize > 0)
      fuse_mount_options.append(base::StringPrintf(",blksize=%d", blksize));

    LOG(INFO) << "Source file " << source << " is a block device, block size "
              << blksize;

    fuse_type = "fuseblk";
  }
  if (!filesystem_type.empty()) {
    fuse_type += ".";
    fuse_type += filesystem_type;
  }

  auto flags = options.ToMountFlagsAndData().first;

  return platform->Mount(source.empty() ? filesystem_type : source,
                         target.value(), fuse_type,
                         flags | kRequiredFuseMountFlags, fuse_mount_options);
}

}  // namespace

FUSEMounter::FUSEMounter(const std::string& source_path,
                         const std::string& target_path,
                         const std::string& filesystem_type,
                         const MountOptions& mount_options,
                         const Platform* platform,
                         brillo::ProcessReaper* process_reaper,
                         const std::string& mount_program_path,
                         const std::string& mount_user,
                         const std::string& seccomp_policy,
                         const std::vector<BindPath>& accessible_paths,
                         bool permit_network_access,
                         const std::string& mount_group)
    : MounterCompat(filesystem_type,
                    source_path,
                    base::FilePath(target_path),
                    mount_options),
      platform_(platform),
      process_reaper_(process_reaper),
      mount_program_path_(mount_program_path),
      mount_user_(mount_user),
      mount_group_(mount_group),
      seccomp_policy_(seccomp_policy),
      accessible_paths_(accessible_paths),
      permit_network_access_(permit_network_access) {}

MountErrorType FUSEMounter::MountImpl() const {
  auto mount_process = CreateSandboxedProcess();
  MountErrorType error = ConfigureCommonSandbox(
      mount_process.get(), platform_, !permit_network_access_,
      base::FilePath(seccomp_policy_));
  if (error != MOUNT_ERROR_NONE) {
    return error;
  }

  uid_t mount_user_id;
  gid_t mount_group_id;
  if (!platform_->GetUserAndGroupId(mount_user_, &mount_user_id,
                                    &mount_group_id)) {
    LOG(ERROR) << "Can't resolve user '" << mount_user_ << "'";
    return MOUNT_ERROR_INTERNAL;
  }
  if (!mount_group_.empty() &&
      !platform_->GetGroupId(mount_group_, &mount_group_id)) {
    return MOUNT_ERROR_INTERNAL;
  }

  mount_process->SetUserId(mount_user_id);
  mount_process->SetGroupId(mount_group_id);

  if (!platform_->PathExists(mount_program_path_)) {
    LOG(ERROR) << "Mount program '" << mount_program_path_ << "' not found.";
    return MOUNT_ERROR_MOUNT_PROGRAM_NOT_FOUND;
  }
  mount_process->AddArgument(mount_program_path_);

  const base::File fuse_file = base::File(
      base::FilePath(kFuseDeviceFile),
      base::File::FLAG_OPEN | base::File::FLAG_READ | base::File::FLAG_WRITE);
  if (!fuse_file.IsValid()) {
    LOG(ERROR) << "Unable to open FUSE device file. Error: "
               << fuse_file.error_details() << " "
               << base::File::ErrorToString(fuse_file.error_details());
    return MOUNT_ERROR_INTERNAL;
  }

  error = MountFuseDevice(platform_, source(), filesystem_type(),
                          base::FilePath(target_path()), fuse_file,
                          mount_user_id, mount_group_id, mount_options());
  if (error != MOUNT_ERROR_NONE) {
    LOG(ERROR) << "Can't perform unprivileged FUSE mount: " << error;
    return error;
  }

  // The |fuse_failure_unmounter| closure runner is used to unmount the FUSE
  // filesystem if any part of starting the FUSE helper process fails.
  // TODO(crbug.com/993857): Use base::BindOnce().
  base::ScopedClosureRunner fuse_cleanup_runner(base::Bind(
      [](const Platform* platform, const std::string& target_path) {
        MountErrorType unmount_error = platform->Unmount(target_path, 0);
        if (unmount_error != MOUNT_ERROR_NONE) {
          LOG(ERROR) << "Failed to unmount a FUSE mount '" << target_path
                     << "': " << unmount_error;
        }
        if (!platform->RemoveEmptyDirectory(target_path)) {
          PLOG(ERROR) << "Couldn't remove FUSE mountpoint '" << target_path;
        }
      },
      platform_, target_path().value()));

  // Source might be an URI. Only try to re-own source if it looks like
  // an existing path.
  if (!source().empty() && platform_->PathExists(source())) {
    if (!platform_->SetOwnership(source(), getuid(), mount_group_id) ||
        !platform_->SetPermissions(source(), kSourcePathPermissions)) {
      LOG(ERROR) << "Can't set up permissions on the source";
      return MOUNT_ERROR_INSUFFICIENT_PERMISSIONS;
    }
  }

  // If a block device is being mounted, bind mount it into the sandbox.
  if (base::StartsWith(source(), "/dev/", base::CompareCase::SENSITIVE)) {
    if (!mount_process->BindMount(source(), source(), true, false)) {
      LOG(ERROR) << "Unable to bind mount device " << source();
      return MOUNT_ERROR_INVALID_ARGUMENT;
    }
  }

  // TODO(crbug.com/933018): Remove when DriveFS helper is refactored.
  if (!mount_process->Mount("tmpfs", "/home", "tmpfs", "mode=0755,size=10M")) {
    LOG(ERROR) << "Can't mount /home";
    return MOUNT_ERROR_INTERNAL;
  }

  // This is for additional data dirs.
  for (const auto& path : accessible_paths_) {
    if (!mount_process->BindMount(path.path, path.path, path.writable,
                                  path.recursive)) {
      LOG(ERROR) << "Can't bind " << path.path;
      return MOUNT_ERROR_INVALID_ARGUMENT;
    }
  }

  std::string options_string = mount_options().ToString();
  if (!options_string.empty()) {
    mount_process->AddArgument("-o");
    mount_process->AddArgument(options_string);
  }
  if (!source().empty()) {
    mount_process->AddArgument(source());
  }
  mount_process->AddArgument(
      base::StringPrintf("/dev/fd/%d", fuse_file.GetPlatformFile()));

  std::vector<std::string> output;
  int return_code = mount_process->Run(&output);
  if (return_code != 0) {
    LOG(ERROR) << "FUSE mount program failed with return code " << return_code;
    if (!output.empty()) {
      LOG(ERROR) << "FUSE mount program outputted " << output.size()
                 << " lines:";
      for (const std::string& line : output) {
        LOG(ERROR) << line;
      }
    }
    return MOUNT_ERROR_MOUNT_PROGRAM_FAILED;
  }

  // At this point, the FUSE daemon has successfully started, so repurpose
  // the FUSE cleanup closure runner to run on daemon quitting.
  // This is defined as in-jail "init" process, denoted by pid(),
  // terminates, which happens only when the last process in the jailed PID
  // namespace terminates.
  process_reaper_->WatchForChild(
      FROM_HERE, mount_process->pid(),
      // TODO(crbug.com/993857): Get rid of base::Passed, when WatchForChild
      // takes base::OnceCallback.
      base::Bind(CleanUpCallback, base::Passed(fuse_cleanup_runner.Release()),
                 target_path()));

  return MOUNT_ERROR_NONE;
}

std::unique_ptr<SandboxedProcess> FUSEMounter::CreateSandboxedProcess() const {
  return std::make_unique<SandboxedProcess>();
}

}  // namespace cros_disks
