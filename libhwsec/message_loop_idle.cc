//
// Copyright 2019 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

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
