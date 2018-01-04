// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "wimax_manager/event_dispatcher.h"

#include <base/location.h>
#include <base/run_loop.h>
#include <base/threading/thread_task_runner_handle.h>

namespace wimax_manager {

EventDispatcher::EventDispatcher()
    : dont_use_directly_(new base::MessageLoopForUI()),
      task_runner_(base::ThreadTaskRunnerHandle::Get()) {}

void EventDispatcher::DispatchForever() {
  base::RunLoop().Run();
}

bool EventDispatcher::PostTask(const base::Closure& task) {
  return task_runner_->PostTask(FROM_HERE, task);
}

bool EventDispatcher::PostDelayedTask(const base::Closure& task,
                                      const base::TimeDelta& delay) {
  return task_runner_->PostDelayedTask(FROM_HERE, task, delay);
}

void EventDispatcher::Stop() {
  // TODO(ejcaruso): move to RunLoop::QuitWhenIdleClosure after libchrome uprev
  base::MessageLoop::current()->task_runner()->PostTask(
      FROM_HERE, base::MessageLoop::QuitWhenIdleClosure());
}

}  // namespace wimax_manager
