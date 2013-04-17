// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LOGIN_MANAGER_MOCK_PROCESS_MANAGER_SERVICE_H_
#define LOGIN_MANAGER_MOCK_PROCESS_MANAGER_SERVICE_H_

#include "login_manager/process_manager_service_interface.h"

#include <base/memory/scoped_ptr.h>
#include <gmock/gmock.h>

namespace login_manager {
class ChildJobInterface;

class MockProcessManagerService : public ProcessManagerServiceInterface {
 public:
  MockProcessManagerService();
  virtual ~MockProcessManagerService();

  MOCK_METHOD0(ScheduleShutdown, void());
  MOCK_METHOD0(RunBrowser, void());
  MOCK_METHOD1(AbortBrowser, void(int));
  MOCK_METHOD1(IsBrowser, bool(pid_t));
  MOCK_METHOD2(RestartBrowserWithArgs, void(const std::vector<std::string>&,
                                            bool));
  MOCK_METHOD1(SetBrowserSessionForUser, void(const std::string&));
  MOCK_METHOD1(RunKeyGenerator, void(const std::string&));

  // gmock can't handle methods that take scoped_ptrs, so we have to build
  // something manually.
  // Call ExpectAdoptAbandonKeyGenerator(pid_t) to make this mock expect:
  //   1) Adoption of the provided pid, and
  //   2) Abandonment.
  void AdoptKeyGeneratorJob(scoped_ptr<ChildJobInterface> job,
                            pid_t pid,
                            guint watcher) OVERRIDE;

  MOCK_METHOD0(AbandonKeyGeneratorJob, void());
  MOCK_METHOD2(ProcessNewOwnerKey, void(const std::string&,
                                        const base::FilePath&));

  void ExpectAdoptAndAbandon(pid_t expected_generator_pid);

 private:
  pid_t generator_pid_;
};
}  // namespace login_manager

#endif  // LOGIN_MANAGER_MOCK_PROCESS_MANAGER_SERVICE_H_
