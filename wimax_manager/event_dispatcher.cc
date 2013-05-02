// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "wimax_manager/event_dispatcher.h"

#include <base/location.h>
#include <base/message_loop_proxy.h>

namespace wimax_manager {

EventDispatcher::EventDispatcher()
    : dont_use_directly_(new(std::nothrow) MessageLoopForUI),
      message_loop_proxy_(base::MessageLoopProxy::current()) {
  CHECK(dont_use_directly_.get());
}

EventDispatcher::~EventDispatcher() {}

void EventDispatcher::DispatchForever() {
  MessageLoop::current()->Run();
}

bool EventDispatcher::PostTask(const base::Closure &task) {
  return message_loop_proxy_->PostTask(FROM_HERE, task);
}

bool EventDispatcher::PostDelayedTask(
    const base::Closure &task, const base::TimeDelta &delay) {
  return message_loop_proxy_->PostDelayedTask(FROM_HERE, task, delay);
}

void EventDispatcher::Stop() {
  MessageLoop::current()->PostTask(FROM_HERE, MessageLoop::QuitClosure());
}

}  // namespace wimax_manager
