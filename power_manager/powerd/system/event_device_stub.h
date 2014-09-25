// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_POWERD_SYSTEM_EVENT_DEVICE_STUB_H_
#define POWER_MANAGER_POWERD_SYSTEM_EVENT_DEVICE_STUB_H_

#include "power_manager/powerd/system/event_device_interface.h"

#include <string>

#include <base/memory/linked_ptr.h>

namespace power_manager {
namespace system {

// TODO(patrikf): Implement EventDeviceStub.

class EventDeviceFactoryStub : public EventDeviceFactoryInterface {
 public:
  EventDeviceFactoryStub();
  virtual ~EventDeviceFactoryStub();

  // Implementation of EventDeviceFactoryInterface.
  linked_ptr<EventDeviceInterface> Open(const std::string& path) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(EventDeviceFactoryStub);
};

}  // namespace system
}  // namespace power_manager

#endif  // POWER_MANAGER_POWERD_SYSTEM_EVENT_DEVICE_STUB_H_
