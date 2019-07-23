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

#ifndef LIBHWSEC_MESSAGE_LOOP_IDLE_H_
#define LIBHWSEC_MESSAGE_LOOP_IDLE_H_

#include <base/bind.h>
#include <base/message_loop/message_loop.h>
#include <base/synchronization/waitable_event.h>

#include <libhwsec/hwsec_export.h>

namespace hwsec {

// MessageLoopIdleEvent: waits for the moment when the message loop becomes
// idle. Note: it is still possible that there are deferred tasks.
//
// Posts the task to the message loop that checks the following:
// If there are tasks in the incoming queue, the loop is not idle, so re-post
// the task.
// If there are no tasks in the incoming queue, it's still possible that there
// are other tasks in the work queue already picked for processing after this
// task. So, in this case, re-post once again, and check the number of
// tasks between now and the next invocation of this task. If only 1 (this
// task only), the task runner is idle.
class HWSEC_EXPORT MessageLoopIdleEvent
    : public base::MessageLoop::TaskObserver {
 public:
  explicit MessageLoopIdleEvent(base::MessageLoop* message_loop);

  ~MessageLoopIdleEvent() = default;

  // Observer callbacks: WillProcessTask and DidProcessTask.
  // Count the number of run tasks in WillProcessTask.
  void WillProcessTask(const base::PendingTask& pending_task) override;
  void DidProcessTask(const base::PendingTask& pending_task) override;

  // The task we put on the message loop.
  void RunTask();

  // Waits until the message loop becomes idle.
  void Wait();
  bool TimedWait(const base::TimeDelta& wait_delta);

 private:
  void PostTask();

  // Event to signal when we detect that the message loop is idle.
  base::WaitableEvent event_;
  // Was observer added to the message loop?
  bool observer_added_;
  // Number of tasks run between previous invocation and now (including this).
  int tasks_processed_;
  // Did the loop appear idle during the previous task invocation?
  bool was_idle_;
  // MessageLoop we are waiting for.
  base::MessageLoop* message_loop_;
};

}  // namespace hwsec

#endif  // LIBHWSEC_MESSAGE_LOOP_IDLE_H_
