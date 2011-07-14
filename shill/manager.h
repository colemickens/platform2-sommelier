// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MANAGER_
#define SHILL_MANAGER_

#include <string>
#include <vector>

#include <base/memory/ref_counted.h>
#include <base/memory/scoped_ptr.h>
#include <chromeos/dbus/service_constants.h>

#include "shill/device.h"
#include "shill/device_info.h"
#include "shill/modem_info.h"
#include "shill/property_store.h"
#include "shill/service.h"
#include "shill/shill_event.h"

namespace shill {

class ControlInterface;
class Error;
class EventDispatcher;
class ManagerAdaptorInterface;
class GLib;

class Manager {
 public:
  struct Properties {
   public:
    Properties() : offline_mode(false) {}
    bool offline_mode;
    std::string check_portal_list;
    std::string country;
    std::string portal_url;
  };

  // A constructor for the Manager object
  Manager(ControlInterface *control_interface,
          EventDispatcher *dispatcher,
          GLib *glib);
  virtual ~Manager();
  void Start();
  void Stop();

  const ProfileRefPtr &ActiveProfile();

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
  std::string GetActiveProfileName();

  void HelpRegisterDerivedString(const std::string &name,
                                 std::string(Manager::*get)(void),
                                 bool(Manager::*set)(const std::string&));
  void HelpRegisterDerivedStrings(const std::string &name,
                                  Strings(Manager::*get)(void),
                                  bool(Manager::*set)(const Strings&));

  scoped_ptr<ManagerAdaptorInterface> adaptor_;
  DeviceInfo device_info_;
  ModemInfo modem_info_;
  bool running_;
  std::vector<DeviceRefPtr> devices_;
  std::vector<ServiceRefPtr> services_;
  std::vector<ProfileRefPtr> profiles_;
  ProfileRefPtr ephemeral_profile_;

  // Properties to be get/set via PropertyStore calls.
  Properties props_;
  PropertyStore store_;

  friend class ManagerAdaptorInterface;
};

}  // namespace shill

#endif  // SHILL_MANAGER_
