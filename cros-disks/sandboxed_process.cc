// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cros-disks/sandboxed_process.h"

#include <stdlib.h>

#include <sys/mount.h>
#include <sys/wait.h>
#include <unistd.h>

#include <base/bind.h>
#include <base/files/file_util.h>
#include <base/files/scoped_file.h>
#include <base/logging.h>
#include <chromeos/libminijail.h>

#include "cros-disks/mount_options.h"
#include "cros-disks/quote.h"
#include "cros-disks/sandboxed_init.h"

namespace cros_disks {
namespace {

int Exec(char* const args[]) {
  const char* const path = args[0];
  execv(path, args);
  PLOG(FATAL) << "Cannot exec " << quote(path);
  return EXIT_FAILURE;
}

}  // namespace

SandboxedProcess::SandboxedProcess() : jail_(minijail_new()) {
  CHECK(jail_) << "Failed to create a process jail";
}

SandboxedProcess::~SandboxedProcess() {
  minijail_destroy(jail_);
}

void SandboxedProcess::LoadSeccompFilterPolicy(const std::string& policy_file) {
  minijail_parse_seccomp_filters(jail_, policy_file.c_str());
  minijail_use_seccomp_filter(jail_);
}

void SandboxedProcess::NewCgroupNamespace() {
  minijail_namespace_cgroups(jail_);
}

void SandboxedProcess::NewIpcNamespace() {
  minijail_namespace_ipc(jail_);
}

void SandboxedProcess::NewMountNamespace() {
  minijail_namespace_vfs(jail_);
}

void SandboxedProcess::NewPidNamespace() {
  minijail_namespace_pids(jail_);
  minijail_run_as_init(jail_);
  minijail_reset_signal_mask(jail_);
  minijail_reset_signal_handlers(jail_);
  run_custom_init_ = true;
}

bool SandboxedProcess::SetUpMinimalMounts() {
  if (minijail_bind(jail_, "/", "/", 0))
    return false;
  if (minijail_bind(jail_, "/proc", "/proc", 0))
    return false;
  minijail_remount_proc_readonly(jail_);
  minijail_mount_tmp(jail_);

  // Create a minimal /dev with a very restricted set of device nodes.
  minijail_mount_dev(jail_);
  return true;
}

bool SandboxedProcess::BindMount(const std::string& from,
                                 const std::string& to,
                                 bool writeable,
                                 bool recursive) {
  MountOptions::Flags flags = MS_BIND;
  if (!writeable) {
    flags |= MS_RDONLY;
  }
  if (recursive) {
    flags |= MS_REC;
  }
  return minijail_mount(jail_, from.c_str(), to.c_str(), "", flags) == 0;
}

bool SandboxedProcess::Mount(const std::string& src,
                             const std::string& to,
                             const std::string& type,
                             const char* data) {
  return minijail_mount_with_data(jail_, src.c_str(), to.c_str(), type.c_str(),
                                  0, data) == 0;
}

bool SandboxedProcess::EnterPivotRoot() {
  return minijail_enter_pivot_root(jail_, "/mnt/empty") == 0;
}

void SandboxedProcess::NewNetworkNamespace() {
  minijail_namespace_net(jail_);
}

void SandboxedProcess::SkipRemountPrivate() {
  minijail_skip_remount_private(jail_);
}

void SandboxedProcess::SetNoNewPrivileges() {
  minijail_no_new_privs(jail_);
}

void SandboxedProcess::SetCapabilities(uint64_t capabilities) {
  minijail_use_caps(jail_, capabilities);
}

void SandboxedProcess::SetGroupId(gid_t group_id) {
  minijail_change_gid(jail_, group_id);
}

void SandboxedProcess::SetUserId(uid_t user_id) {
  minijail_change_uid(jail_, user_id);
}

void SandboxedProcess::CloseOpenFds() {
  minijail_close_open_fds(jail_);
}

bool SandboxedProcess::PreserveFile(const base::File& file) {
  return minijail_preserve_fd(jail_, file.GetPlatformFile(),
                              file.GetPlatformFile()) == 0;
}

int SandboxedProcess::WaitAll() {
  if (!run_custom_init_) {
    return Wait();
  }
  return WaitImpl();
}

pid_t SandboxedProcess::StartImpl(base::ScopedFD* in_fd,
                                  base::ScopedFD* out_fd,
                                  base::ScopedFD* err_fd) {
  char* const* const args = GetArguments();
  DCHECK(args && args[0]);
  pid_t child_pid = kInvalidProcessId;
  if (!run_custom_init_) {
    int in = kInvalidFD, out = kInvalidFD, err = kInvalidFD;
    if (minijail_run_pid_pipes(jail_, args[0], args, &child_pid, &in, &out,
                               &err) != 0) {
      return kInvalidProcessId;
    }
    in_fd->reset(in);
    out_fd->reset(out);
    err_fd->reset(err);
  } else {
    SandboxedInit init_;
    child_pid = minijail_fork(jail_);
    if (child_pid == kInvalidProcessId) {
      return kInvalidProcessId;
    }
    if (child_pid == 0) {
      init_.RunInsideSandboxNoReturn(base::Bind(Exec, args));
    } else {
      custom_init_control_fd_ = init_.TakeInitControlFD(in_fd, out_fd, err_fd);
      CHECK(base::SetNonBlocking(custom_init_control_fd_.get()));
    }
  }
  return child_pid;
}

int SandboxedProcess::WaitImpl() {
  return minijail_wait(jail_);
}

bool SandboxedProcess::WaitNonBlockingImpl(int* status) {
  if (run_custom_init_ && PollStatus(status)) {
    *status = SandboxedInit::WStatusToStatus(*status);
    return true;
  }

  // Minijail didn't implement the non-blocking wait.
  // Code below is stolen from minijail_wait() with addition of WNOHANG.
  int ret = waitpid(pid(), status, WNOHANG);
  if (ret < 0) {
    PLOG(ERROR) << "waitpid failed.";
    return true;
  }

  if (ret == 0) {
    return false;
  }

  if (WIFEXITED(*status) || WIFSIGNALED(*status)) {
    *status = SandboxedInit::WStatusToStatus(*status);
    return true;
  }

  return false;
}

bool SandboxedProcess::PollStatus(int* status) {
  return SandboxedInit::PollLauncherStatus(&custom_init_control_fd_, status);
}

}  // namespace cros_disks
