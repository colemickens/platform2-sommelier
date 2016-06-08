// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LOGIN_MANAGER_MOCK_CONTAINER_MANAGER_H_
#define LOGIN_MANAGER_MOCK_CONTAINER_MANAGER_H_

#include <stdint.h>
#include <unistd.h>

#include <string>
#include <vector>

#include <gmock/gmock.h>

#include "login_manager/container_manager_interface.h"

namespace login_manager {

class MockContainerManager : public ContainerManagerInterface {
 public:
  MockContainerManager() {}
  ~MockContainerManager() override {}

  MOCK_METHOD1(IsManagedJob, bool(pid_t pid));
  MOCK_METHOD1(HandleExit, void(const siginfo_t& status));
  MOCK_METHOD0(RequestJobExit, void());
  MOCK_METHOD1(EnsureJobExit, void(base::TimeDelta timeout));

  MOCK_METHOD0(StartContainer, bool());
  MOCK_CONST_METHOD1(GetRootFsPath, bool(base::FilePath* path_out));
  MOCK_CONST_METHOD1(GetContainerPID, bool(pid_t* pid_out));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockContainerManager);
};

}  // namespace login_manager

#endif  // LOGIN_MANAGER_MOCK_CONTAINER_MANAGER_H_
