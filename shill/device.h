// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_DEVICE_
#define SHILL_DEVICE_

#include <vector>

#include <base/memory/ref_counted.h>

#include "shill/service.h"
#include "shill/shill_event.h"

namespace shill {

class ControlInterface;
class DeviceAdaptorInterface;
class EventDispatcher;

// Device superclass.  Individual network interfaces types will inherit from
// this class.
class Device : public base::RefCounted<Device> {
 public:
  enum Technology {
    kEthernet,
    kWifi,
    kCellular,
    kNumTechnologies
  };

  // A constructor for the Device object
  Device(ControlInterface *control_interface,
         EventDispatcher *dispatcher);
  virtual ~Device();

  virtual void Start();
  virtual void Stop();

  virtual bool TechnologyIs(Technology type) = 0;

 protected:
  std::vector<scoped_refptr<Service> > services_;

 private:
  DeviceAdaptorInterface *adaptor_;
  bool running_;
  friend class DeviceAdaptorInterface;
  DISALLOW_COPY_AND_ASSIGN(Device);
};

}  // namespace shill

#endif  // SHILL_DEVICE_
