// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LOGIN_MANAGER_VPD_PROCESS_H_
#define LOGIN_MANAGER_VPD_PROCESS_H_

#include <string>
#include <vector>

#include "login_manager/policy_service.h"

namespace login_manager {

class VpdProcess {
 public:
  // Run VPD setter script as a separate process. Takes ownership of
  // |completion| if process starts successfully. Returns whether fork() was
  // successful.
  virtual bool RunInBackground(const std::vector<std::string>& flags,
                               const std::vector<int>& values,
                               bool is_enrolled,
                               const PolicyService::Completion& completion) = 0;
};

}  // namespace login_manager

#endif  // LOGIN_MANAGER_VPD_PROCESS_H_
