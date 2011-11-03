// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MANAGER_
#define SHILL_MANAGER_

#include <string>
#include <vector>

#include <base/file_path.h>
#include <base/memory/ref_counted.h>
#include <base/memory/scoped_ptr.h>
#include <chromeos/dbus/service_constants.h>
#include <gtest/gtest_prod.h>  // for FRIEND_TEST

#include "shill/device.h"
#include "shill/device_info.h"
#include "shill/event_dispatcher.h"
#include "shill/modem_info.h"
#include "shill/property_store.h"
#include "shill/service.h"
#include "shill/wifi.h"

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

  Manager(ControlInterface *control_interface,
          EventDispatcher *dispatcher,
          GLib *glib,
          const std::string &run_directory,
          const std::string &storage_directory,
          const std::string &user_storage_format);
  virtual ~Manager();

  void AddDeviceToBlackList(const std::string &device_name);
  void Start();
  void Stop();

  const ProfileRefPtr &ActiveProfile();
  bool MoveServiceToProfile(const ServiceRefPtr &to_move,
                            const ProfileRefPtr &destination);

  void RegisterDevice(const DeviceRefPtr &to_manage);
  void DeregisterDevice(const DeviceRefPtr &to_forget);

  virtual void RegisterService(const ServiceRefPtr &to_manage);
  virtual void DeregisterService(const ServiceRefPtr &to_forget);
  virtual void UpdateService(const ServiceConstRefPtr &to_update);

  void FilterByTechnology(Technology::Identifier tech,
                          std::vector<DeviceRefPtr> *found);

  ServiceRefPtr FindService(const std::string& name);
  std::vector<std::string> EnumerateAvailableServices();

  // called via RPC (e.g., from ManagerDBusAdaptor)
  WiFiServiceRefPtr GetWifiService(const KeyValueStore &args, Error *error);
  void RequestScan(const std::string &technology, Error *error);
  std::string GetTechnologyOrder();
  void SetTechnologyOrder(const std::string &order, Error *error);
  // Create a profile.  This does not affect the profile stack.
  void CreateProfile(const std::string &name, Error *error);
  // Pushes existing profile with name |name| onto stack of managed profiles.
  void PushProfile(const std::string &name, Error *error);
  // Pops profile named |name| off the top of the stack of managed profiles.
  void PopProfile(const std::string &name, Error *error);
  // Remove the active profile.
  void PopAnyProfile(Error *error);

  virtual DeviceInfo *device_info() { return &device_info_; }
  PropertyStore *mutable_store() { return &store_; }
  virtual const PropertyStore &store() const { return store_; }

  std::vector<DeviceRefPtr>::iterator devices_begin() {
    return devices_.begin();
  }
  std::vector<DeviceRefPtr>::iterator devices_end() { return devices_.end(); }

 private:
  friend class ManagerAdaptorInterface;
  friend class ManagerTest;
  FRIEND_TEST(ManagerTest, PushPopProfile);
  FRIEND_TEST(ManagerTest, SortServices);

  static const char kManagerErrorNoDevice[];

  std::string CalculateState();
  std::vector<std::string> AvailableTechnologies();
  std::vector<std::string> ConnectedTechnologies();
  std::string DefaultTechnology();
  std::vector<std::string> EnabledTechnologies();
  std::vector<std::string> EnumerateDevices();
  // TODO(cmasone): This should be implemented by filtering |services_|.
  std::vector<std::string> EnumerateWatchedServices();
  std::string GetActiveProfileName();

  void HelpRegisterDerivedString(
      const std::string &name,
      std::string(Manager::*get)(void),
      void(Manager::*set)(const std::string&, Error *));
  void HelpRegisterDerivedStrings(
      const std::string &name,
      Strings(Manager::*get)(void),
      void(Manager::*set)(const Strings&, Error *));

  void PopProfileInternal();
  bool OrderServices(ServiceRefPtr a, ServiceRefPtr b);
  void SortServices();

  const FilePath run_path_;
  const FilePath storage_path_;
  const std::string user_storage_format_;
  scoped_ptr<ManagerAdaptorInterface> adaptor_;
  DeviceInfo device_info_;
  ModemInfo modem_info_;
  bool running_;
  // Used to facilitate unit tests which can't use RPC.
  bool connect_profiles_to_rpc_;
  std::vector<DeviceRefPtr> devices_;
  // We store Services in a vector, because we want to keep them sorted.
  std::vector<ServiceRefPtr> services_;
  std::vector<ProfileRefPtr> profiles_;
  ProfileRefPtr ephemeral_profile_;
  ControlInterface *control_interface_;
  GLib *glib_;

  // The priority order of technologies
  std::vector<Technology::Identifier> technology_order_;

  // Properties to be get/set via PropertyStore calls.
  Properties props_;
  PropertyStore store_;
};

}  // namespace shill

#endif  // SHILL_MANAGER_
