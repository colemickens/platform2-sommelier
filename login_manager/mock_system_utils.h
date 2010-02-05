// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LOGIN_MANAGER_MOCK_SYSTEM_UTILS_H_
#define LOGIN_MANAGER_MOCK_SYSTEM_UTILS_H_

#include "login_manager/system_utils.h"

#include <unistd.h>
#include <gmock/gmock.h>

namespace login_manager {
class MockSystemUtils : public SystemUtils {
 public:
  MockSystemUtils() {}
  ~MockSystemUtils() {}
  MOCK_METHOD2(kill, int(pid_t pid, int signal));
  MOCK_METHOD2(child_is_gone, bool(pid_t child_spec, int timeout));
};
}  // namespace login_manager

#endif  // LOGIN_MANAGER_MOCK_SYSTEM_UTILS_H_
