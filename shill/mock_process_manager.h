// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MOCK_PROCESS_MANAGER_H_
#define SHILL_MOCK_PROCESS_MANAGER_H_

#include "shill/process_manager.h"

#include <map>
#include <string>
#include <vector>

#include <base/location.h>
#include <base/macros.h>
#include <gmock/gmock.h>

namespace shill {

class MockProcessManager : public ProcessManager {
 public:
  MockProcessManager();
  ~MockProcessManager() override;

  MOCK_METHOD1(Init, void(EventDispatcher* dispatcher));
  MOCK_METHOD0(Stop, void());
  MOCK_METHOD6(StartProcess,
               pid_t(const base::Location& spawn_source,
                     const base::FilePath& program,
                     const std::vector<std::string>& arguments,
                     const std::map<std::string, std::string>& env,
                     bool terminate_with_parent,
                     const base::Callback<void(int)>& exit_callback));
  MOCK_METHOD9(StartProcessInMinijail,
               pid_t(const base::Location& spawn_source,
                     const base::FilePath& program,
                     const std::vector<std::string>& arguments,
                     const std::string& user,
                     const std::string& group,
                     uint64_t capmask,
                     bool inherit_supplementary_groups,
                     bool close_nonstd_fds,
                     const base::Callback<void(int)>& exit_callback));
  MOCK_METHOD10(StartProcessInMinijailWithPipes,
                pid_t(const base::Location& spawn_source,
                      const base::FilePath& program,
                      const std::vector<std::string>& arguments,
                      const std::string& user,
                      const std::string& group,
                      uint64_t capmask,
                      bool inherit_supplementary_groups,
                      bool close_nonstd_fds,
                      const base::Callback<void(int)>& exit_callback,
                      struct std_file_descriptors std_fds));
  MOCK_METHOD1(StopProcess, bool(pid_t pid));
  MOCK_METHOD1(StopProcessAndBlock, bool(pid_t pid));
  MOCK_METHOD2(UpdateExitCallback,
               bool(pid_t pid, const base::Callback<void(int)>& new_callback));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockProcessManager);
};

}  // namespace shill

#endif  // SHILL_MOCK_PROCESS_MANAGER_H_
