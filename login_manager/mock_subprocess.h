// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LOGIN_MANAGER_MOCK_SUBPROCESS_H_
#define LOGIN_MANAGER_MOCK_SUBPROCESS_H_

#include <stdint.h>
#include <unistd.h>

#include <string>
#include <vector>

#include <base/macros.h>
#include <gmock/gmock.h>

#include "login_manager/subprocess.h"

namespace login_manager {

class MockSubprocess : public SubprocessInterface {
 public:
  MockSubprocess();
  ~MockSubprocess() override;

  MOCK_METHOD2(ForkAndExec,
               bool(const std::vector<std::string>& args,
                    const std::vector<std::string>& env_vars));
  MOCK_METHOD1(Kill, void(int signal));
  MOCK_METHOD1(KillEverything, void(int signal));
  MOCK_CONST_METHOD0(GetPid, pid_t());
  MOCK_METHOD0(ClearPid, void());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockSubprocess);
};

}  // namespace login_manager

#endif  // LOGIN_MANAGER_MOCK_SUBPROCESS_H_
