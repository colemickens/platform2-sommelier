// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/powerd/system/event_device_stub.h"

#include <base/logging.h>

namespace power_manager {
namespace system {

EventDeviceFactoryStub::EventDeviceFactoryStub() {}
EventDeviceFactoryStub::~EventDeviceFactoryStub() {}

linked_ptr<EventDeviceInterface> EventDeviceFactoryStub::Open(
    const std::string& path) {
  NOTIMPLEMENTED();
  return linked_ptr<EventDeviceInterface>();
}

}  // namespace system
}  // namespace power_manager
