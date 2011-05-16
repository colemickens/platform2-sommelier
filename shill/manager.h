// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MANAGER_
#define SHILL_MANAGER_

#include <string>
#include <vector>

#include <base/memory/ref_counted.h>
#include <base/memory/scoped_ptr.h>

#include "shill/device.h"
#include "shill/device_info.h"
#include "shill/service.h"
#include "shill/shill_event.h"

namespace shill {

class Manager {
 public:
  // A constructor for the Manager object
  Manager(ControlInterface *control_interface,
          EventDispatcher *dispatcher);
  ~Manager();
  void Start();
  void Stop();

  void RegisterDevice(Device *to_manage);
  void DeregisterDevice(const Device *to_forget);

  void RegisterService(Service *to_manage);
  void DeregisterService(const Service *to_forget);

  void FilterByTechnology(Device::Technology tech,
                          std::vector<scoped_refptr<Device> > *found);

  Service* FindService(const std::string& name);

 private:
  scoped_ptr<ManagerAdaptorInterface> adaptor_;
  DeviceInfo device_info_;
  bool running_;
  std::vector<scoped_refptr<Device> > devices_;
  std::vector<scoped_refptr<Service> > services_;
  friend class ManagerAdaptorInterface;
};

}  // namespace shill

#endif  // SHILL_MANAGER_
