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

class MockSystemUtils : public SystemUtilsImpl {
 public:
  MockSystemUtils();
  ~MockSystemUtils() override;

  MOCK_METHOD3(kill, int(pid_t pid, uid_t uid, int signal));
  MOCK_METHOD1(time, time_t(time_t*));  // NOLINT
  MOCK_METHOD0(fork, pid_t(void));
  MOCK_METHOD0(GetDevModeState, DevModeState(void));
  MOCK_METHOD2(ProcessGroupIsGone, bool(pid_t child_spec,
                                        base::TimeDelta timeout));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockSystemUtils);
};

}  // namespace login_manager

#endif  // LOGIN_MANAGER_MOCK_SYSTEM_UTILS_H_
