// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libweave/examples/ubuntu/event_task_runner.h"

#include <signal.h>

namespace weave {
namespace examples {

namespace {
event_base* g_event_base = nullptr;
}

void EventTaskRunner::PostDelayedTask(
    const tracked_objects::Location& from_here,
    const base::Closure& task,
    base::TimeDelta delay) {
  base::Time new_time = base::Time::Now() + delay;
  if (queue_.empty() || new_time < queue_.top().first.first) {
    ReScheduleEvent(delay);
  }
  queue_.emplace(std::make_pair(new_time, ++counter_), task);
}

void EventTaskRunner::Run() {
  g_event_base = base_.get();

  struct sigaction sa = {};
  sa.sa_handler = [](int signal) {
    event_base_loopexit(g_event_base, nullptr);
  };
  sigfillset(&sa.sa_mask);
  sigaction(SIGINT, &sa, nullptr);

  event_base_loop(g_event_base, EVLOOP_NO_EXIT_ON_EMPTY);
  g_event_base = nullptr;
}

void EventTaskRunner::ReScheduleEvent(base::TimeDelta delay) {
  timespec ts = delay.ToTimeSpec();
  timeval tv = {ts.tv_sec, ts.tv_nsec / 1000};
  event_add(task_event_.get(), &tv);
}

void EventTaskRunner::EventHandler(int, int16_t, void* runner) {
  static_cast<EventTaskRunner*>(runner)->Process();
}

void EventTaskRunner::FreeEvent(event* evnt) {
  event_del(evnt);
  event_free(evnt);
}

void EventTaskRunner::Process() {
  while (!queue_.empty() && queue_.top().first.first <= base::Time::Now()) {
    auto cb = queue_.top().second;
    queue_.pop();
    cb.Run();
  }
  if (!queue_.empty()) {
    base::TimeDelta delta = std::max(
        base::TimeDelta(), queue_.top().first.first - base::Time::Now());
    ReScheduleEvent(delta);
  }
}

}  // namespace examples
}  // namespace weave
