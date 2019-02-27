// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chaps/dbus/scoped_bus.h"

#include <base/bind.h>
#include <base/location.h>
#include <base/single_thread_task_runner.h>
#include <base/synchronization/waitable_event.h>
#include <base/threading/thread_task_runner_handle.h>

namespace chaps {

void ShutdownBusOnTaskRunner(scoped_refptr<dbus::Bus> bus,
                             base::WaitableEvent* completion_event) {
  bus->ShutdownAndBlock();
  completion_event->Signal();
}

ScopedBus::ScopedBus() : bus_(nullptr), task_runner_(nullptr) {}

ScopedBus::ScopedBus(const dbus::Bus::Options& bus_options)
    : bus_(new dbus::Bus(bus_options)) {
  // Tests may not have a task runner, but only use one thread.
  if (base::ThreadTaskRunnerHandle::IsSet())
    task_runner_ = base::ThreadTaskRunnerHandle::Get();
}

ScopedBus::ScopedBus(ScopedBus&& other)
    : bus_(other.bus_), task_runner_(other.task_runner_) {
  other.bus_ = nullptr;
  other.task_runner_ = nullptr;
}

ScopedBus::~ScopedBus() {
  if (!bus_)
    return;

  if (!task_runner_ || task_runner_->BelongsToCurrentThread()) {
    bus_->ShutdownAndBlock();
  } else {
    base::WaitableEvent completion_event(
        base::WaitableEvent::ResetPolicy::AUTOMATIC,
        base::WaitableEvent::InitialState::NOT_SIGNALED);
    task_runner_->PostTask(FROM_HERE, base::Bind(&ShutdownBusOnTaskRunner, bus_,
                                                 &completion_event));
    completion_event.Wait();
  }
}

ScopedBus& ScopedBus::operator=(ScopedBus&& other) {
  bus_ = other.bus_;
  task_runner_ = other.task_runner_;
  other.bus_ = nullptr;
  other.task_runner_ = nullptr;
  return *this;
}

}  // namespace chaps
