// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LOGIN_MANAGER_MOCK_SESSION_MANAGER_H_
#define LOGIN_MANAGER_MOCK_SESSION_MANAGER_H_

#include "login_manager/session_manager_interface.h"

#include <string>
#include <vector>

#include <gmock/gmock.h>

namespace login_manager {

class MockSessionManager : public SessionManagerInterface {
 public:
  MockSessionManager();
  virtual ~MockSessionManager();

  MOCK_METHOD0(Initialize, bool());
  MOCK_METHOD0(Finalize, void());
  MOCK_METHOD0(GetStartUpFlags, std::vector<std::string>());
  MOCK_METHOD0(AnnounceSessionStoppingIfNeeded, void());
  MOCK_METHOD0(AnnounceSessionStopped, void());
  MOCK_METHOD0(ShouldEndSession, bool());
  MOCK_METHOD0(InitiateDeviceWipe, void());
};
}  // namespace login_manager

#endif  // LOGIN_MANAGER_MOCK_SESSION_MANAGER_H_
