// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "login_manager/fake_termina_manager.h"

namespace login_manager {

FakeTerminaManager::FakeTerminaManager() {}

FakeTerminaManager::~FakeTerminaManager() {}

bool FakeTerminaManager::StartVmContainer(const base::FilePath& image_path,
                                          const std::string& name,
                                          bool writable) {
  return true;
}

bool FakeTerminaManager::StopVmContainer(const std::string& name) {
  return true;
}

bool FakeTerminaManager::IsManagedJob(pid_t pid) {
  return true;
}

void FakeTerminaManager::HandleExit(const siginfo_t& status) {}

void FakeTerminaManager::RequestJobExit(const std::string& reason) {}

void FakeTerminaManager::EnsureJobExit(base::TimeDelta timeout) {}

}  // namespace login_manager
