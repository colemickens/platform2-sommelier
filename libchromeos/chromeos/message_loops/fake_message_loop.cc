// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <chromeos/message_loops/fake_message_loop.h>

#include <base/logging.h>

namespace chromeos {


FakeMessageLoop::FakeMessageLoop(base::SimpleTestClock* clock)
  : test_clock_(clock) {
}

MessageLoop::TaskId FakeMessageLoop::PostDelayedTask(const base::Closure &task,
                                                     base::TimeDelta delay) {
  // If no SimpleTestClock was provided, we use the last time we fired a
  // callback. In this way, tasks scheduled from a Closure will have the right
  // time.
  if (test_clock_)
    current_time_ = test_clock_->Now();
  MessageLoop::TaskId current_id = ++last_id_;
  if (!current_id)
    current_id = ++last_id_;
  tasks_[current_id] = task;
  fire_order_.push(std::make_pair(current_time_ + delay, current_id));
  return current_id;
}

bool FakeMessageLoop::CancelTask(TaskId task_id) {
  if (task_id == MessageLoop::kTaskIdNull)
    return false;
  return tasks_.erase(task_id) > 0;
}

bool FakeMessageLoop::RunOnce(bool may_block) {
  if (test_clock_)
    current_time_ = test_clock_->Now();
  while (!fire_order_.empty() &&
         (may_block || fire_order_.top().first <= current_time_)) {
    const auto task_ref = fire_order_.top();
    fire_order_.pop();
    // We need to skip tasks in the priority_queue not in the |tasks_| map.
    // This is normal if the task was canceled, as there is no efficient way
    // to remove a task from the priority_queue.
    const auto task = tasks_.find(task_ref.second);
    if (task == tasks_.end())
      continue;
    // Advance the clock to the task firing time, if needed.
    if (current_time_ < task_ref.first) {
      current_time_ = task_ref.first;
      if (test_clock_)
        test_clock_->SetNow(current_time_);
    }
    // Move the Closure out of the map before delete it. We need to delete the
    // entry from the map before we call the callback, since calling CancelTask
    // for the task you are running now should fail and return false.
    base::Closure callback = std::move(task->second);
    tasks_.erase(task);

    callback.Run();
    return true;
  }
  return false;
}

bool FakeMessageLoop::PendingTasks() {
  for (const auto& task : tasks_) {
    LOG(INFO) << "Pending task_id " << task.first
              << " at address " << &task.second;
  }
  return !tasks_.empty();
}

}  // namespace chromeos
