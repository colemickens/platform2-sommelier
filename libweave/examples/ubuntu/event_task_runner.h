// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBWEAVE_EXAMPLES_UBUNTU_EVENT_TASK_RUNNER_H_
#define LIBWEAVE_EXAMPLES_UBUNTU_EVENT_TASK_RUNNER_H_

#include <queue>
#include <utility>
#include <vector>

#include <event2/event.h>

#include <weave/task_runner.h>

namespace weave {
namespace examples {

// Simple task runner implemented with libevent message loop.
class EventTaskRunner : public TaskRunner {
 public:
  void PostDelayedTask(const tracked_objects::Location& from_here,
                       const base::Closure& task,
                       base::TimeDelta delay) override;

  event_base* GetEventBase() const { return base_.get(); }

  void Run();

 private:
  void ReScheduleEvent(base::TimeDelta delay);
  static void EventHandler(int, int16_t, void* runner);
  static void FreeEvent(event* evnt);
  void Process();

  using QueueItem = std::pair<std::pair<base::Time, size_t>, base::Closure>;

  struct Greater {
    bool operator()(const QueueItem& a, const QueueItem& b) const {
      return a.first > b.first;
    }
  };

  size_t counter_{0};  // Keeps order of tasks with the same time.

  std::priority_queue<QueueItem,
                      std::vector<QueueItem>,
                      EventTaskRunner::Greater> queue_;

  std::unique_ptr<event_base, decltype(&event_base_free)> base_{
      event_base_new(), &event_base_free};
  std::unique_ptr<event, decltype(&FreeEvent)> task_event_{
      event_new(base_.get(), -1, EV_TIMEOUT, &EventHandler, this), &FreeEvent};
};

}  // namespace examples
}  // namespace weave

#endif  // LIBWEAVE_EXAMPLES_UBUNTU_EVENT_TASK_RUNNER_H_
