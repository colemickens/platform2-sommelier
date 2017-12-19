// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LOGIN_MANAGER_FAKE_TERMINA_MANAGER_H_
#define LOGIN_MANAGER_FAKE_TERMINA_MANAGER_H_

#include <string>

#include "login_manager/termina_manager_interface.h"

namespace login_manager {

// Fake implementation used for tests.
class FakeTerminaManager : public TerminaManagerInterface {
 public:
  FakeTerminaManager();
  ~FakeTerminaManager() override;

  // TerminaManagerInterface:
  bool StartVmContainer(const base::FilePath& image_path,
                        const std::string& name,
                        bool writable) override;
  bool StopVmContainer(const std::string& name) override;

  // JobManagerInterface:
  bool IsManagedJob(pid_t pid) override;
  void HandleExit(const siginfo_t& status) override;
  void RequestJobExit(const std::string& reason) override;
  void EnsureJobExit(base::TimeDelta timeout) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeTerminaManager);
};

}  // namespace login_manager

#endif  // LOGIN_MANAGER_FAKE_TERMINA_MANAGER_H_
