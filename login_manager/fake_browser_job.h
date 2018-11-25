// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LOGIN_MANAGER_FAKE_BROWSER_JOB_H_
#define LOGIN_MANAGER_FAKE_BROWSER_JOB_H_

#include "login_manager/browser_job.h"

#include <sys/types.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <base/time/time.h>
#include <gmock/gmock.h>

namespace login_manager {
class FakeChildProcess;

class FakeBrowserJob : public BrowserJobInterface {
 public:
  explicit FakeBrowserJob(const std::string& name);
  FakeBrowserJob(const std::string& name, bool schedule_exit);
  ~FakeBrowserJob() override;

  void set_fake_child_process(std::unique_ptr<FakeChildProcess> fake) {
    fake_process_ = std::move(fake);
  }
  void set_should_run(bool should) { should_run_ = should; }

  // Overridden from BrowserJobInterface
  bool IsGuestSession() override;
  bool ShouldRunBrowser() override;
  MOCK_CONST_METHOD0(ShouldStop, bool());
  MOCK_METHOD2(KillEverything, void(int, const std::string&));
  MOCK_METHOD2(Kill, void(int, const std::string&));
  MOCK_METHOD1(WaitAndAbort, void(base::TimeDelta));
  MOCK_METHOD2(StartSession, void(const std::string&, const std::string&));
  MOCK_METHOD0(StopSession, void());
  MOCK_METHOD1(SetArguments, void(const std::vector<std::string>&));
  MOCK_METHOD1(SetExtraArguments, void(const std::vector<std::string>&));
  MOCK_METHOD1(SetExtraEnvironmentVariables,
               void(const std::vector<std::string>&));
  MOCK_METHOD1(SetOneTimeArguments, void(const std::vector<std::string>&));
  MOCK_METHOD0(ClearOneTimeArguments, void());

  bool RunInBackground() override;
  const std::string GetName() const override;
  pid_t CurrentPid() const override;
  void ClearPid() override;

 private:
  std::unique_ptr<FakeChildProcess> fake_process_;
  const std::string name_;
  bool running_ = false;
  const bool schedule_exit_;
  bool should_run_ = true;
  DISALLOW_COPY_AND_ASSIGN(FakeBrowserJob);
};
}  // namespace login_manager

#endif  // LOGIN_MANAGER_FAKE_BROWSER_JOB_H_
