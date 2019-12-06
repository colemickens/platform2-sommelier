// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libhwsec/message_loop_idle.h"

namespace hwsec {

MessageLoopIdleEvent::MessageLoopIdleEvent(base::MessageLoop* message_loop)
    : event_(base::WaitableEvent::ResetPolicy::MANUAL,
             base::WaitableEvent::InitialState::NOT_SIGNALED),
      observer_added_(false),
      tasks_processed_(0),
      was_idle_(false),
      message_loop_(message_loop) {
  PostTask();
}

void MessageLoopIdleEvent::WillProcessTask(
    const base::PendingTask& pending_task) {
  tasks_processed_++;
}

void MessageLoopIdleEvent::DidProcessTask(
    const base::PendingTask& pending_task) {}

void MessageLoopIdleEvent::RunTask() {
  // We need to add the observer in RunTask, since it can only be done by the
  // thread that runs MessageLoop
  if (!observer_added_) {
    message_loop_->AddTaskObserver(this);
    observer_added_ = true;
  }
  bool is_idle = (tasks_processed_ <= 1) && message_loop_->IsIdleForTesting();
  if (was_idle_ && is_idle) {
    // We need to remove the observer in RunTask, since it can only be done by
    // the thread that runs MessageLoop
    if (observer_added_) {
      message_loop_->RemoveTaskObserver(this);
      observer_added_ = false;
    }
    event_.Signal();
    return;
  }
  was_idle_ = is_idle;
  tasks_processed_ = 0;
  PostTask();
}

void MessageLoopIdleEvent::Wait() {
  event_.Wait();
}

bool MessageLoopIdleEvent::TimedWait(const base::TimeDelta& wait_delta) {
  return event_.TimedWait(wait_delta);
}

void MessageLoopIdleEvent::PostTask() {
  auto task =
      base::Bind(&MessageLoopIdleEvent::RunTask, base::Unretained(this));
  message_loop_->task_runner()->PostTask(FROM_HERE, task);
}

}  // namespace hwsec
