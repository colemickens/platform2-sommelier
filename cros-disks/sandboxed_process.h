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
  virtual ~SandboxedProcess();

  // Loads the seccomp filters from |policy_file|. The calling process will be
  // aborted if |policy_file| does not exist, cannot be read or is malformed.
  void LoadSeccompFilterPolicy(const std::string& policy_file);

  // Sets the process capabilities of the process to be sandboxed.
  void SetCapabilities(uint64_t capabilities);

  // Sets the group ID of the process to be sandboxed.
  void SetGroupId(gid_t group_id);

  // Sets the user ID of the process to be sandboxed.
  void SetUserId(uid_t user_id);

  // Implements Process::Start() to start the process in a sandbox.
  // Returns true on success.
  virtual bool Start();

  // Waits for the process to finish and returns its exit status.
  int Wait();

  // Starts and waits for the process to finish. Returns the same exit status
  // as Wait() does.
  int Run();

 private:
  struct minijail* jail_;

  DISALLOW_COPY_AND_ASSIGN(SandboxedProcess);
};

}  // namespace cros_disks

#endif  // CROS_DISKS_SANDBOXED_PROCESS_H_
