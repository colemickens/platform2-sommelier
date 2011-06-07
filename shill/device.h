// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_DEVICE_
#define SHILL_DEVICE_

#include <string>
#include <vector>

#include <base/memory/ref_counted.h>
#include <base/memory/scoped_ptr.h>

#include "shill/device_config_interface.h"
#include "shill/service.h"
#include "shill/shill_event.h"

namespace shill {

class ControlInterface;
class Device;
class DeviceAdaptorInterface;
class DeviceInfo;
class EventDispatcher;
class Endpoint;
class Manager;

typedef scoped_refptr<const Device> DeviceConstRefPtr;
typedef scoped_refptr<Device> DeviceRefPtr;

// Device superclass.  Individual network interfaces types will inherit from
// this class.
class Device : public DeviceConfigInterface {
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
         Manager *manager,
         const std::string& link_name,
         int interface_index);
  virtual ~Device();

  virtual void Start();
  virtual void Stop();

  virtual bool TechnologyIs(const Technology type) = 0;
  virtual void LinkEvent(unsigned flags, unsigned change);
  virtual void Scan();

  // Implementation of DeviceConfigInterface
  virtual void ConfigIP() {}

  // Returns a string that is guaranteed to uniquely identify this
  // Device instance.
  const std::string& UniqueName() const;

 protected:
  std::vector<ServiceRefPtr> services_;
  std::string link_name_;
  int interface_index_;
  bool running_;
  Manager *manager_;

 private:
  scoped_ptr<DeviceAdaptorInterface> adaptor_;
  friend class DeviceAdaptorInterface;
  DISALLOW_COPY_AND_ASSIGN(Device);
};

}  // namespace shill

#endif  // SHILL_DEVICE_
