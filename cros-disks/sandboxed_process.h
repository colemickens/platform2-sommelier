// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CROS_DISKS_SANDBOXED_PROCESS_H_
#define CROS_DISKS_SANDBOXED_PROCESS_H_

#include <sys/types.h>

#include <string>

#include "cros-disks/process.h"

struct minijail;

namespace cros_disks {

class SandboxedProcess : public Process {
 public:
  SandboxedProcess();
  ~SandboxedProcess() override;

  // Loads the seccomp filters from |policy_file|. The calling process will be
  // aborted if |policy_file| does not exist, cannot be read or is malformed.
  void LoadSeccompFilterPolicy(const std::string& policy_file);

  // Puts the process to be sandboxed in a new cgroup namespace.
  void NewCgroupNamespace();

  // Puts the process to be sandboxed in a new IPC namespace.
  void NewIpcNamespace();

  // Puts the process to be sandboxed in a new mount namespace.
  void NewMountNamespace();

  // Puts the process to be sandboxed in a new network namespace.
  void NewNetworkNamespace();

  // Assuming the process is sandboxed in a new mount namespace, some essential
  // mountpoints like / and /proc are being set up.
  bool SetUpMinimalMounts();

  // Maps a file or a folder into process' mount namespace.
  bool BindMount(const std::string& from,
                 const std::string& to,
                 bool writeable,
                 bool recursive);

  // Mounts source to the specified folder in the new mount namespace.
  bool Mount(const std::string& src,
             const std::string& to,
             const std::string& type,
             const char* data);

  // Makes the process to call pivot_root for an empty /.
  bool EnterPivotRoot();

  // Skips re-marking existing mounts as private.
  void SkipRemountPrivate();

  // Sets the no_new_privs bit.
  void SetNoNewPrivileges();

  // Sets the process capabilities of the process to be sandboxed.
  void SetCapabilities(uint64_t capabilities);

  // Sets the group ID of the process to be sandboxed.
  void SetGroupId(gid_t group_id);

  // Sets the user ID of the process to be sandboxed.
  void SetUserId(uid_t user_id);

  // Implements Process::Start() to start the process in a sandbox.
  // Returns true on success.
  bool Start() override;

  // Waits for the process to finish and returns its exit status.
  int Wait() override;

 private:
  minijail* jail_;

  DISALLOW_COPY_AND_ASSIGN(SandboxedProcess);
};

}  // namespace cros_disks

#endif  // CROS_DISKS_SANDBOXED_PROCESS_H_
