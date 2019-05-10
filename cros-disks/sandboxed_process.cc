// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cros-disks/sandboxed_process.h"

#include <sys/mount.h>
#include <sys/wait.h>

#include <base/logging.h>
#include <chromeos/libminijail.h>

#include "cros-disks/mount_options.h"

namespace cros_disks {

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

pid_t SandboxedProcess::StartImpl(std::vector<char*>& args,
                                  base::ScopedFD* in_fd,
                                  base::ScopedFD* out_fd,
                                  base::ScopedFD* err_fd) {
  char** arguments = args.data();
  pid_t child_pid = kInvalidProcessId;
  int in = kInvalidFD, out = kInvalidFD, err = kInvalidFD;
  int result = minijail_run_pid_pipes(jail_, arguments[0], arguments,
                                      &child_pid, &in, &out, &err);
  if (result == 0) {
    in_fd->reset(in);
    out_fd->reset(out);
    err_fd->reset(err);
    return child_pid;
  }

  return kInvalidProcessId;
}

int SandboxedProcess::WaitImpl() {
  return minijail_wait(jail_);
}

bool SandboxedProcess::WaitNonBlockingImpl(int* status) {
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

  if (WIFEXITED(*status)) {
    *status = WEXITSTATUS(*status);
  } else if (WIFSIGNALED(*status)) {
    // Also from minijail_wait().
    *status = 128 + WTERMSIG(*status);
  }
  return true;
}

}  // namespace cros_disks
