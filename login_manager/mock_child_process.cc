// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "login_manager/mock_child_process.h"

namespace login_manager {

MockChildProcess::MockChildProcess(pid_t pid,
                                   int status,
                                   SessionManagerService::TestApi api)
    : pid_(pid),
      exit_status_(status),
      test_api_(api) {
}

MockChildProcess::~MockChildProcess() {}

void MockChildProcess::ScheduleExit() {
  test_api_.ScheduleChildExit(pid_, exit_status_);
}

}  // namespace login_manager
