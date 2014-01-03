// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LOGIN_MANAGER_FAKE_GENERATOR_JOB_H_
#define LOGIN_MANAGER_FAKE_GENERATOR_JOB_H_

#include "login_manager/generator_job.h"

#include <sys/types.h>

#include <gmock/gmock.h>
#include <string>
#include <vector>

namespace login_manager {
class FakeGeneratorJob : public GeneratorJobInterface {
 public:
  FakeGeneratorJob(pid_t pid, const std::string& name);
  virtual ~FakeGeneratorJob();

  MOCK_METHOD0(RunInBackground, bool());
  MOCK_METHOD2(KillEverything, void(int, const std::string&));
  MOCK_METHOD2(Kill, void(int, const std::string&));

  const std::string GetName() const OVERRIDE { return name_; }

  pid_t CurrentPid() const OVERRIDE { return pid_; }

 private:
  pid_t pid_;
  const std::string name_;
  DISALLOW_COPY_AND_ASSIGN(FakeGeneratorJob);
};
}  // namespace login_manager

#endif  // LOGIN_MANAGER_FAKE_GENERATOR_JOB_H_
