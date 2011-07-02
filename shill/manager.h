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
#include "shill/property_store.h"
#include "shill/service.h"
#include "shill/shill_event.h"

namespace shill {

class ControlInterface;
class Error;
class EventDispatcher;
class ManagerAdaptorInterface;

class Manager {
 public:
  // A constructor for the Manager object
  Manager(ControlInterface *control_interface,
          EventDispatcher *dispatcher);
  virtual ~Manager();
  void Start();
  void Stop();

  void RegisterDevice(const DeviceRefPtr &to_manage);
  void DeregisterDevice(const DeviceConstRefPtr &to_forget);

  void RegisterService(const ServiceRefPtr &to_manage);
  void DeregisterService(const ServiceConstRefPtr &to_forget);

  void FilterByTechnology(Device::Technology tech,
                          std::vector<DeviceRefPtr> *found);

  ServiceRefPtr FindService(const std::string& name);

  PropertyStore *store() { return &store_; }

 private:
  std::string CalculateState();
  std::vector<std::string> AvailableTechnologies();
  std::vector<std::string> ConnectedTechnologies();
  std::string DefaultTechnology();
  std::vector<std::string> EnabledTechnologies();
  std::vector<std::string> EnumerateDevices();
  // TODO(cmasone): these two should be implemented by asking the currently
  // active profile, once we have such a thing.
  std::vector<std::string> EnumerateAvailableServices();
  std::vector<std::string> EnumerateWatchedServices();

  void HelpRegisterDerivedString(const std::string &name,
                             std::string(Manager::*get)(void),
                             bool(Manager::*set)(const std::string&));
  void HelpRegisterDerivedStrings(const std::string &name,
                              Strings(Manager::*get)(void),
                              bool(Manager::*set)(const Strings&));

  scoped_ptr<ManagerAdaptorInterface> adaptor_;
  DeviceInfo device_info_;
  bool running_;
  std::vector<DeviceRefPtr> devices_;
  std::vector<ServiceRefPtr> services_;

  // Properties to be get/set via PropertyStore calls.
  bool offline_mode_;
  std::string state_;
  std::string check_portal_list_;
  std::string country_;
  std::string portal_url_;

  PropertyStore store_;

  std::string active_profile_;  // This is supposed to be, essentially,
                                // an RPC-visible object handle

  friend class ManagerAdaptorInterface;
};

}  // namespace shill

#endif  // SHILL_MANAGER_
