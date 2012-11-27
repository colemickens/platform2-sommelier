// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/common/test_main_loop_runner.h"

#include "base/logging.h"

namespace power_manager {

TestMainLoopRunner::TestMainLoopRunner(const base::TimeDelta& timeout_delay)
    : loop_(g_main_loop_new(NULL, FALSE)),
      timeout_delay_(timeout_delay),
      timeout_id_(0),
      timed_out_(false) {
}

TestMainLoopRunner::~TestMainLoopRunner() {
  if (timeout_id_)
    g_source_remove(timeout_id_);
  g_main_loop_unref(loop_);
  loop_ = NULL;
}

bool TestMainLoopRunner::StartLoop() {
  CHECK(!timeout_id_) << "Loop is already running";
  timeout_id_ =
      g_timeout_add(timeout_delay_.InMilliseconds(), OnTimeoutThunk, this);
  timed_out_ = false;
  g_main_loop_run(loop_);
  return !timed_out_;
}

void TestMainLoopRunner::StopLoop() {
  if (timeout_id_) {
    g_source_remove(timeout_id_);
    timeout_id_ = 0;
  }
  g_main_loop_quit(loop_);
}

gboolean TestMainLoopRunner::OnTimeout() {
  timeout_id_ = 0;
  timed_out_ = true;
  g_main_loop_quit(loop_);
  return FALSE;
}

}  // namespace power_manager
