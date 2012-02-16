// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/event_dispatcher.h"

#include <stdio.h>
#include <glib.h>

#include <base/callback.h>
#include <base/location.h>
#include <base/message_loop_proxy.h>

#include "shill/glib_io_input_handler.h"
#include "shill/glib_io_ready_handler.h"

using base::Callback;
using base::Closure;

namespace shill {

EventDispatcher::EventDispatcher()
    : dont_use_directly_(new MessageLoopForUI),
      message_loop_proxy_(base::MessageLoopProxy::current()) {
}

EventDispatcher::~EventDispatcher() {}

void EventDispatcher::DispatchForever() {
  MessageLoop::current()->Run();
}

void EventDispatcher::DispatchPendingEvents() {
  MessageLoop::current()->RunAllPending();
}

bool EventDispatcher::PostTask(const Closure &task) {
  return message_loop_proxy_->PostTask(FROM_HERE, task);
}

bool EventDispatcher::PostDelayedTask(const Closure &task, int64 delay_ms) {
  return message_loop_proxy_->PostDelayedTask(FROM_HERE, task, delay_ms);
}

IOHandler *EventDispatcher::CreateInputHandler(
    int fd,
    const Callback<void(InputData *)> &callback) {
  IOHandler *handler = new GlibIOInputHandler(fd, callback);
  handler->Start();
  return handler;
}

IOHandler *EventDispatcher::CreateReadyHandler(
    int fd,
    IOHandler::ReadyMode mode,
    const Callback<void(int)> &callback) {
  IOHandler *handler = new GlibIOReadyHandler(fd, mode, callback);
  handler->Start();
  return handler;
}

}  // namespace shill
