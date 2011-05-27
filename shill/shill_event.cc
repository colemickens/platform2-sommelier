// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>
#include <glib.h>

#include <base/callback_old.h>
#include <base/message_loop_proxy.h>

#include "shill/glib_io_handler.h"
#include "shill/shill_event.h"

namespace shill {

EventDispatcher::EventDispatcher()
    : dont_use_directly_(new MessageLoopForUI),
      message_loop_proxy_(base::MessageLoopProxy::CreateForCurrentThread()) {
}

EventDispatcher::~EventDispatcher() {}

void EventDispatcher::DispatchForever() {
  MessageLoop::current()->Run();
}

bool EventDispatcher::PostTask(Task* task) {
  return message_loop_proxy_->PostTask(FROM_HERE, task);
}

bool EventDispatcher::PostDelayedTask(Task* task, int64 delay_ms) {
  return message_loop_proxy_->PostDelayedTask(FROM_HERE, task, delay_ms);
}

IOInputHandler *EventDispatcher::CreateInputHandler(
    int fd,
    Callback1<InputData*>::Type *callback) {
  return new GlibIOInputHandler(fd, callback);
}

}  // namespace shill
