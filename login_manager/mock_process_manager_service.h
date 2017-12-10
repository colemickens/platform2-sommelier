// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LOGIN_MANAGER_MOCK_PROCESS_MANAGER_SERVICE_H_
#define LOGIN_MANAGER_MOCK_PROCESS_MANAGER_SERVICE_H_

#include "login_manager/process_manager_service_interface.h"

#include <string>
#include <vector>

#include <gmock/gmock.h>

namespace login_manager {
class GeneratorJobInterface;

class MockProcessManagerService : public ProcessManagerServiceInterface {
 public:
  MockProcessManagerService();
  virtual ~MockProcessManagerService();

  MOCK_METHOD0(ScheduleShutdown, void());
  MOCK_METHOD0(ShouldRunBrowser, bool());
  MOCK_METHOD0(RunBrowser, void());
  MOCK_METHOD2(AbortBrowser, void(int, const std::string&));
  MOCK_METHOD3(RestartBrowserWithArgs,
               void(const std::vector<std::string>&,
                    bool,
                    const std::vector<std::string>&));
  MOCK_METHOD2(SetBrowserSessionForUser,
               void(const std::string&, const std::string&));
  MOCK_METHOD2(SetFlagsForUser,
               void(const std::string& username,
                    const std::vector<std::string>& flags));
  MOCK_METHOD1(IsBrowser, bool(pid_t));
  MOCK_METHOD1(IsManagedProcess, bool(pid_t));
};
}  // namespace login_manager

#endif  // LOGIN_MANAGER_MOCK_PROCESS_MANAGER_SERVICE_H_
