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
#include "shill/property_store_interface.h"
#include "shill/service.h"
#include "shill/shill_event.h"

namespace shill {

class ControlInterface;
class Error;
class EventDispatcher;
class ManagerAdaptorInterface;

class Manager : public PropertyStoreInterface {
 public:
  // A constructor for the Manager object
  Manager(ControlInterface *control_interface,
          EventDispatcher *dispatcher);
  virtual ~Manager();
  void Start();
  void Stop();

  void RegisterDevice(DeviceRefPtr to_manage);
  void DeregisterDevice(DeviceConstRefPtr to_forget);

  void RegisterService(ServiceRefPtr to_manage);
  void DeregisterService(ServiceConstRefPtr to_forget);

  void FilterByTechnology(Device::Technology tech,
                          std::vector<DeviceRefPtr> *found);

  ServiceRefPtr FindService(const std::string& name);

  // Implementation of PropertyStoreInterface
  virtual bool Contains(const std::string &property);
  virtual bool SetBoolProperty(const std::string &name,
                               bool value,
                               Error *error);
  virtual bool SetStringProperty(const std::string &name,
                                 const std::string &value,
                                 Error *error);

 private:
  scoped_ptr<ManagerAdaptorInterface> adaptor_;
  DeviceInfo device_info_;
  bool running_;
  std::vector<std::string> known_properties_;
  std::vector<DeviceRefPtr> devices_;
  std::vector<ServiceRefPtr> services_;

  // Properties to be get/set via PropertyStoreInterface calls.
  bool offline_mode_;
  std::string state_;
  std::string country_;
  std::string portal_url_;

  std::string active_profile_;  // This is supposed to be, essentially,
                                // an RPC-visible object handle

  friend class ManagerAdaptorInterface;
};

}  // namespace shill

#endif  // SHILL_MANAGER_
