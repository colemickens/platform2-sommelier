// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_COMMON_TEST_MAIN_LOOP_RUNNER_H_
#define POWER_MANAGER_COMMON_TEST_MAIN_LOOP_RUNNER_H_

#include <glib.h>

#include "base/basictypes.h"
#include "base/time.h"
#include "power_manager/common/signal_callback.h"

namespace power_manager {

// This class makes it easier to write tests that need to wait for an
// asynchronous event to be run by the GLib event loop.  The typical usage
// pattern is as follows:
//
// 1. Instantiate a TestMainLoopRunner.
// 2. Set up a callback for the asynchronous event that calls StopLoop().
// 3. Call StartLoop() and test that it returns true.
// 4. Test that the asynchronous event included the expected data.
class TestMainLoopRunner {
 public:
  explicit TestMainLoopRunner(const base::TimeDelta& timeout_delay);
  ~TestMainLoopRunner();

  // Runs |loop_| until StopLoop() is called or |timeout_delay_| has elapsed.
  // Returns true if the loop was stopped via StopLoop() or false if the timeout
  // was hit.
  bool StartLoop();

  // Stops |loop_|, resulting in control returning to StartLoop() (which will
  // return true).
  void StopLoop();

 private:
  // Called when |timeout_delay_| elapses without the loop having been stopped.
  SIGNAL_CALLBACK_0(TestMainLoopRunner,
                    gboolean,
                    OnTimeout);

  GMainLoop* loop_;

  // Delay after which OnTimeout() will be called.
  base::TimeDelta timeout_delay_;

  // GLib event source ID for running OnTimeout().  0 if not running.
  guint timeout_id_;

  // Was |loop_| stopped as a result of OnTimeout() being called rather than
  // StopLoop()?
  bool timed_out_;

  DISALLOW_COPY_AND_ASSIGN(TestMainLoopRunner);
};

}  // namespace power_manager

#endif  // POWER_MANAGER_COMMON_TEST_MAIN_LOOP_RUNNER_H_
