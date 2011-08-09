// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_STUB_DEVICE_
#define SHILL_STUB_DEVICE_

#include <base/memory/ref_counted.h>
#include <base/memory/scoped_ptr.h>

#include <vector>

#include "shill/device.h"
#include "shill/service.h"
#include "shill/shill_event.h"

namespace shill {

class ControlInterface;
class DeviceAdaptorInterface;
class EventDispatcher;
class Endpoint;
class DeviceInfo;
class Manager;

// Non-functional Device subclass used for non-operable or blacklisted devices
class DeviceStub : public Device {
 public:
  DeviceStub(ControlInterface *control_interface,
             EventDispatcher *dispatcher,
             Manager *manager,
             const std::string& link_name,
             int interface_index,
             Technology technology)
      : Device(control_interface, dispatcher, manager, link_name,
               interface_index),
        technology_(technology) {}
  void Start() {}
  void Stop() {}
  bool TechnologyIs(const Technology type) const { return type == technology_; }

 private:
  Technology technology_;

  DISALLOW_COPY_AND_ASSIGN(DeviceStub);
};

}  // namespace shill

#endif  // SHILL_STUB_DEVICE_
