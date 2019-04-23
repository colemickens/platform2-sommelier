// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEBUGD_SRC_MOCK_PROCESS_WITH_ID_H_
#define DEBUGD_SRC_MOCK_PROCESS_WITH_ID_H_

#include <stdint.h>

#include <string>
#include <vector>

#include <gmock/gmock.h>

#include "debugd/src/process_with_id.h"

namespace debugd {
class MockProcessWithId : public ProcessWithId {
 public:
  MOCK_METHOD0(Init, bool());
  MOCK_METHOD1(Init, bool(const std::vector<std::string>& minijail_extra_args));
  MOCK_METHOD0(DisableSandbox, void());
  MOCK_METHOD2(SandboxAs,
               void(const std::string& user, const std::string& group));
  MOCK_METHOD0(InheritUsergroups, void());
  MOCK_METHOD1(SetCapabilities, void(uint64_t capabilities_mask));
  MOCK_METHOD1(SetSeccompFilterPolicyFile, void(const std::string& path));
  MOCK_METHOD0(AllowAccessRootMountNamespace, void());
  MOCK_METHOD0(KillProcessGroup, bool());
  MOCK_METHOD1(AddArg, void(const std::string& arg));
  MOCK_METHOD1(RedirectInput, void(const std::string& input_file));
  MOCK_METHOD1(RedirectOutput, void(const std::string& output_file));
  MOCK_METHOD2(RedirectUsingPipe, void(int child_fd, bool is_input));
  MOCK_METHOD2(BindFd, void(int parent_fd, int child_fd));
  MOCK_METHOD1(SetUid, void(uid_t));
  MOCK_METHOD1(SetGid, void(gid_t));
  MOCK_METHOD1(ApplySyscallFilter, void(const std::string& path));
  MOCK_METHOD0(EnterNewPidNamespace, void());
  MOCK_METHOD1(SetInheritParentSignalMask, void(bool));
  MOCK_METHOD1(SetPreExecCallback, void(const PreExecCallback&));
  MOCK_METHOD1(SetSearchPath, void(bool));
  MOCK_METHOD1(GetPipe, int(int child_fd));
  MOCK_METHOD0(Start, bool());
  MOCK_METHOD0(Wait, int());
  MOCK_METHOD0(Run, int());
  MOCK_METHOD0(pid, pid_t());
  MOCK_METHOD2(Kill, bool(int signal, int timeout));
  MOCK_METHOD1(Reset, void(pid_t));
  MOCK_METHOD1(ResetPidByFile, bool(const std::string& pid_file));
  MOCK_METHOD0(Release, pid_t());
  MOCK_METHOD1(SetCloseUnusedFileDescriptors, void(bool close_unused_fds));
};
}  // namespace debugd
#endif  // DEBUGD_SRC_MOCK_PROCESS_WITH_ID_H_
