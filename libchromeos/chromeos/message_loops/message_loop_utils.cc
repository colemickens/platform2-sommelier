// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <chromeos/message_loops/message_loop_utils.h>

#include <chromeos/bind_lambda.h>

namespace chromeos {

void MessageLoopRunUntil(
    MessageLoop* loop,
    base::TimeDelta timeout,
    base::Callback<bool()> terminate) {
  bool timeout_called = false;
  MessageLoop::TaskId task_id = loop->PostDelayedTask(
    base::Bind([&timeout_called]() { timeout_called = true; }),
    timeout);
  while (!timeout_called && (terminate.is_null() || !terminate.Run()))
    loop->RunOnce(true);

  if (!timeout_called)
    loop->CancelTask(task_id);
}

int MessageLoopRunMaxIterations(MessageLoop* loop, int iterations) {
  int result;
  for (result = 0; result < iterations && loop->RunOnce(false); result++) {}
  return result;
}

}  // namespace chromeos
