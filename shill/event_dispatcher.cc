// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/event_dispatcher.h"

#include <stdio.h>

#include <base/callback.h>
#include <base/location.h>
#include <base/message_loop/message_loop.h>
#include <base/run_loop.h>
#include <base/time/time.h>

using base::Callback;
using base::Closure;
using tracked_objects::Location;

namespace shill {

EventDispatcher::EventDispatcher()
    : io_handler_factory_(
          IOHandlerFactoryContainer::GetInstance()->GetIOHandlerFactory()) {
}

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

// TODO(zqiu): Remove all reference to this function and use the
// IOHandlerFactory function directly. Delete this function once
// all references are removed.
IOHandler* EventDispatcher::CreateReadyHandler(
    int fd,
    IOHandler::ReadyMode mode,
    const Callback<void(int)>& ready_callback) {
  return io_handler_factory_->CreateIOReadyHandler(
          fd, mode, ready_callback);
}

}  // namespace shill
