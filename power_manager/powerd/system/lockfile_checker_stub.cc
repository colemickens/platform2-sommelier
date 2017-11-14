// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/powerd/system/lockfile_checker_stub.h"

namespace power_manager {
namespace system {

LockfileCheckerStub::LockfileCheckerStub() = default;

LockfileCheckerStub::~LockfileCheckerStub() = default;

std::vector<base::FilePath> LockfileCheckerStub::GetValidLockfiles() const {
  return files_to_return_;
}

}  // namespace system
}  // namespace power_manager
