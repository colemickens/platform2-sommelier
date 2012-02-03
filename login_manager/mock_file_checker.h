// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LOGIN_MANAGER_MOCK_FILE_CHECKER_H_
#define LOGIN_MANAGER_MOCK_FILE_CHECKER_H_

#include "login_manager/file_checker.h"

#include <base/file_path.h>
#include <gmock/gmock.h>

namespace login_manager {
class MockFileChecker : public FileChecker {
 public:
  explicit MockFileChecker(std::string filename);
  ~MockFileChecker();
  MOCK_METHOD0(exists, bool());
};
}  // namespace login_manager

#endif  // LOGIN_MANAGER_MOCK_FILE_CHECKER_H_
