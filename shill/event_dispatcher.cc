// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/event_dispatcher.h"

#include <glib.h>
#include <stdio.h>

#include <base/callback.h>
#include <base/location.h>
#include <base/message_loop/message_loop_proxy.h>
#include <base/run_loop.h>
#include <base/time/time.h>

using base::Callback;
using base::Closure;

namespace shill {

EventDispatcher::EventDispatcher()
    : io_handler_factory_(
          IOHandlerFactoryContainer::GetInstance()->GetIOHandlerFactory()) {
}

EventDispatcher::~EventDispatcher() {}

void EventDispatcher::DispatchForever() {
  base::MessageLoop::current()->Run();
}

void EventDispatcher::DispatchPendingEvents() {
  base::RunLoop().RunUntilIdle();
}

bool EventDispatcher::PostTask(const Closure& task) {
  return base::MessageLoopProxy::current()->PostTask(FROM_HERE, task);
}

bool EventDispatcher::PostDelayedTask(const Closure& task, int64_t delay_ms) {
  return base::MessageLoopProxy::current()->PostDelayedTask(
      FROM_HERE, task, base::TimeDelta::FromMilliseconds(delay_ms));
}

// TODO(zqiu): Remove all reference to this function and use the
// IOHandlerFactory function directly. Delete this function once
// all references are removed.
IOHandler* EventDispatcher::CreateInputHandler(
    int fd,
    const IOHandler::InputCallback& input_callback,
    const IOHandler::ErrorCallback& error_callback) {
  return io_handler_factory_->CreateIOInputHandler(
          fd, input_callback, error_callback);
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
