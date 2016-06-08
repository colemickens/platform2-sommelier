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

class MockSessionContainers : public SessionContainersInterface {
 public:
  MockSessionContainers() {}
  ~MockSessionContainers() override {}

  MOCK_METHOD1(StartContainer, bool(const std::string& name));
  MOCK_METHOD1(WaitForContainerToExit, bool(const std::string& name));
  MOCK_METHOD1(KillContainer, bool(const std::string& name));
  MOCK_METHOD0(KillAllContainers, bool(void));

  MOCK_CONST_METHOD2(GetRootFsPath, bool(const std::string& name,
                                         base::FilePath* path_out));
  MOCK_CONST_METHOD2(GetContainerPID, bool(const std::string& name,
                                           pid_t* pid_out));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockSessionContainers);
};

}  // namespace login_manager

#endif  // LOGIN_MANAGER_MOCK_CONTAINER_MANAGER_H_
