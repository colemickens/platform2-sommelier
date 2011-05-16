// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_DEVICE_
#define SHILL_DEVICE_

#include <vector>

#include <base/memory/ref_counted.h>
#include <base/memory/scoped_ptr.h>

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
    kBlackListed,
    kUnknown,
    kNumTechnologies
  };

  // A constructor for the Device object
  Device(ControlInterface *control_interface,
         EventDispatcher *dispatcher,
         const string &link_name,
         int interface_index);
  virtual ~Device();

  virtual void Start();
  virtual void Stop();

  virtual bool TechnologyIs(const Technology type) = 0;

 protected:
  std::vector<scoped_refptr<Service> > services_;
  string link_name_;
  int interface_index_;
  bool running_;

 private:
  scoped_ptr<DeviceAdaptorInterface> adaptor_;
  friend class base::RefCounted<Device>;
  friend class DeviceAdaptorInterface;
  DISALLOW_COPY_AND_ASSIGN(Device);
};

// Non-functional Device subclass used for non-operable or blacklisted devices
class StubDevice : public Device {
 public:
  StubDevice(ControlInterface *control_interface,
             EventDispatcher *dispatcher,
             const string link_name,
             int interface_index,
             Technology technology)
    : Device(control_interface, dispatcher, link_name, interface_index),
    technology_(technology) {}
  void Start() {}
  void Stop() {}
  bool TechnologyIs(const Technology type) { return type == technology_; }

 private:
  Technology technology_;
  DISALLOW_COPY_AND_ASSIGN(StubDevice);
};

}  // namespace shill

#endif  // SHILL_DEVICE_
