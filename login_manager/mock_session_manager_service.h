// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LOGIN_MANAGER_MOCK_SESSION_MANAGER_SERVICE_H_
#define LOGIN_MANAGER_MOCK_SESSION_MANAGER_SERVICE_H_

#include "login_manager/session_manager_service.h"

#include <gmock/gmock.h>

namespace login_manager {

class MockSessionManagerService : public SessionManagerService {
 public:
  MockSessionManagerService();
  virtual ~MockSessionManagerService();
  MOCK_METHOD0(AbortBrowser, void());
};
}  // namespace login_manager

#endif  // LOGIN_MANAGER_MOCK_SESSION_MANAGER_SERVICE_H_
