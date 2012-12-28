// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/common/test_main_loop_runner.h"

#include "base/logging.h"
#include "power_manager/common/util.h"

namespace power_manager {

TestMainLoopRunner::TestMainLoopRunner()
    : loop_(g_main_loop_new(NULL, FALSE)),
      timeout_id_(0),
      timed_out_(false) {
}

TestMainLoopRunner::~TestMainLoopRunner() {
  util::RemoveTimeout(&timeout_id_);
  g_main_loop_unref(loop_);
  loop_ = NULL;
}

bool TestMainLoopRunner::StartLoop(base::TimeDelta timeout_delay) {
  CHECK(!timeout_id_) << "Loop is already running";
  timeout_id_ =
      g_timeout_add(timeout_delay.InMilliseconds(), OnTimeoutThunk, this);
  timed_out_ = false;
  g_main_loop_run(loop_);
  return !timed_out_;
}

void TestMainLoopRunner::StopLoop() {
  util::RemoveTimeout(&timeout_id_);
  g_main_loop_quit(loop_);
}

gboolean TestMainLoopRunner::OnTimeout() {
  timeout_id_ = 0;
  timed_out_ = true;
  g_main_loop_quit(loop_);
  return FALSE;
}

}  // namespace power_manager
