// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LOGIN_MANAGER_MOCK_SYSTEM_UTILS_H_
#define LOGIN_MANAGER_MOCK_SYSTEM_UTILS_H_

#include <stdint.h>
#include <unistd.h>

#include <string>

#include <base/macros.h>
#include <gmock/gmock.h>

#include "login_manager/system_utils_impl.h"

namespace login_manager {

// TODO(yusukes): Derive from the interface class (SystemUtils) instead.
class MockSystemUtils : public SystemUtilsImpl {
 public:
  MockSystemUtils();
  ~MockSystemUtils() override;

  MOCK_METHOD3(kill, int(pid_t pid, uid_t uid, int signal));
  MOCK_METHOD1(time, time_t(time_t*));  // NOLINT
  MOCK_METHOD0(fork, pid_t(void));
  MOCK_METHOD0(GetDevModeState, DevModeState(void));
  MOCK_METHOD0(GetVmState, VmState(void));
  MOCK_METHOD2(ProcessGroupIsGone, bool(pid_t child_spec,
                                        base::TimeDelta timeout));
  MOCK_METHOD1(AmountOfFreeDiskSpace, int64_t(const base::FilePath& path));
  MOCK_METHOD2(GetGroupInfo, bool(const std::string& group_name,
                                  gid_t* out_gid));
  MOCK_METHOD3(ChangeOwner, bool(const base::FilePath& filename,
                                 pid_t pid,
                                 gid_t gid));
  MOCK_METHOD2(SetPosixFilePermissions, bool(const base::FilePath& filename,
                                             mode_t mode));
  MOCK_METHOD1(CreateServerHandle,
               ScopedPlatformHandle(const NamedPlatformHandle& named_handle));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockSystemUtils);
};

}  // namespace login_manager

#endif  // LOGIN_MANAGER_MOCK_SYSTEM_UTILS_H_
