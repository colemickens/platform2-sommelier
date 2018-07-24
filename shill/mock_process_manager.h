// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MOCK_PROCESS_MANAGER_H_
#define SHILL_MOCK_PROCESS_MANAGER_H_

#include "shill/process_manager.h"

#include <map>
#include <string>
#include <vector>

#include <base/macros.h>
#include <gmock/gmock.h>

namespace shill {

class MockProcessManager : public ProcessManager {
 public:
  MockProcessManager();
  ~MockProcessManager() override;

  MOCK_METHOD1(Init, void(EventDispatcher* dispatcher));
  MOCK_METHOD6(StartProcess,
               pid_t(const tracked_objects::Location& from_here,
                     const base::FilePath& program,
                     const std::vector<std::string>& arguments,
                     const std::map<std::string, std::string>& env,
                     bool terminate_with_parent,
                     const base::Callback<void(int)>& exit_callback));
  MOCK_METHOD7(StartProcessInMinijail,
               pid_t(const tracked_objects::Location& from_here,
                     const base::FilePath& program,
                     const std::vector<std::string>& arguments,
                     const std::string& user,
                     const std::string& group,
                     uint64_t capmask,
                     const base::Callback<void(int)>& exit_callback));
  MOCK_METHOD1(StopProcess, bool(pid_t pid));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockProcessManager);
};

}  // namespace shill

#endif  // SHILL_MOCK_PROCESS_MANAGER_H_
