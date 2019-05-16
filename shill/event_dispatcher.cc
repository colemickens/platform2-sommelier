// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/event_dispatcher.h"

#include <base/message_loop/message_loop.h>
#include <base/run_loop.h>
#include <base/time/time.h>

using base::Callback;
using base::Closure;
using tracked_objects::Location;

namespace shill {

EventDispatcher::EventDispatcher() = default;

EventDispatcher::~EventDispatcher() = default;

void EventDispatcher::DispatchForever() {
  base::RunLoop().Run();
}

void EventDispatcher::DispatchPendingEvents() {
  base::RunLoop().RunUntilIdle();
}

void EventDispatcher::PostTask(const Location& location, const Closure& task) {
  base::MessageLoop::current()->task_runner()->PostTask(location, task);
}

void EventDispatcher::PostDelayedTask(const Location& location,
                                      const Closure& task, int64_t delay_ms) {
  base::MessageLoop::current()->task_runner()->PostDelayedTask(
      location, task, base::TimeDelta::FromMilliseconds(delay_ms));
}

}  // namespace shill
