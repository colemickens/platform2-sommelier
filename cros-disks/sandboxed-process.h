// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CROS_DISKS_SANDBOXED_PROCESS_H_
#define CROS_DISKS_SANDBOXED_PROCESS_H_

#include <sys/types.h>

#include <string>
#include <vector>

#include <base/basictypes.h>
#include <base/memory/scoped_ptr.h>
#include <gtest/gtest_prod.h>

struct minijail;

namespace cros_disks {

class SandboxedProcess {
 public:
  SandboxedProcess();
  ~SandboxedProcess();

  // Adds an argument to the end of the argument list.
  void AddArgument(const std::string& argument);

  // Loads the seccomp filters from |policy_file|. The calling process will be
  // aborted if |policy_file| does not exist, cannot be read or is malformed.
  void LoadSeccompFilterPolicy(const std::string& policy_file);

  // Sets the process capabilities of the process to be sandboxed.
  void SetCapabilities(uint64_t capabilities);

  // Sets the group ID of the process to be sandboxed.
  void SetGroupId(gid_t group_id);

  // Sets the user ID of the process to be sandboxed.
  void SetUserId(uid_t user_id);

  // Starts the process in a sandbox. Returns true on success.
  bool Start();

  // Waits for the process to finish and returns its exit status.
  int Wait();

  // Starts and waits for the process to finish. Returns the same exit status
  // as Wait() does.
  int Run();

 private:
  // Builds the array of arguments |arguments_array_|, which is passed to
  // the process to be launched by Start(), from the arguments stored in
  // |arguments_|.
  void BuildArgumentsArray();

  std::vector<std::string> arguments_;

  scoped_array<char*> arguments_array_;

  scoped_array<char> arguments_buffer_;

  struct minijail* jail_;

  FRIEND_TEST(SandboxedProcessTest, BuildEmptyArgumentsArray);
  FRIEND_TEST(SandboxedProcessTest, BuildArgumentsArray);

  DISALLOW_COPY_AND_ASSIGN(SandboxedProcess);
};

}  // namespace cros_disks

#endif  // CROS_DISKS_SANDBOXED_PROCESS_H_
