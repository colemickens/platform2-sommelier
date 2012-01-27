// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
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
class GLib;
class ManagerAdaptorInterface;
class Metrics;

class Manager {
 public:
  struct Properties {
   public:
    Properties() : offline_mode(false) {}
    bool offline_mode;
    std::string check_portal_list;
    std::string country;
    std::string portal_url;
    std::string host_name;
  };

  Manager(ControlInterface *control_interface,
          EventDispatcher *dispatcher,
          Metrics *metrics,
          GLib *glib,
          const std::string &run_directory,
          const std::string &storage_directory,
          const std::string &user_storage_format);
  virtual ~Manager();

  void AddDeviceToBlackList(const std::string &device_name);
  void Start();
  void Stop();

  const ProfileRefPtr &ActiveProfile() const;
  bool IsActiveProfile(const ProfileRefPtr &profile) const;
  bool MoveServiceToProfile(const ServiceRefPtr &to_move,
                            const ProfileRefPtr &destination);

  // Called via RPC call on Service (|to_set|) to set the "Profile" property.
  void SetProfileForService(const ServiceRefPtr &to_set,
                            const std::string &profile,
                            Error *error);

  void RegisterDevice(const DeviceRefPtr &to_manage);
  void DeregisterDevice(const DeviceRefPtr &to_forget);

  virtual bool HasService(const ServiceRefPtr &service);
  // Register a Service with the Manager. Manager may choose to
  // connect to it immediately.
  virtual void RegisterService(const ServiceRefPtr &to_manage);
  // Deregister a Service from the Manager. Caller is responsible
  // for disconnecting the Service before-hand.
  virtual void DeregisterService(const ServiceRefPtr &to_forget);
  virtual void UpdateService(const ServiceRefPtr &to_update);

  void FilterByTechnology(Technology::Identifier tech,
                          std::vector<DeviceRefPtr> *found);

  ServiceRefPtr FindService(const std::string& name);
  std::vector<std::string> EnumerateAvailableServices(Error *error);

  // called via RPC (e.g., from ManagerDBusAdaptor)
  WiFiServiceRefPtr GetWifiService(const KeyValueStore &args, Error *error);
  void RequestScan(const std::string &technology, Error *error);
  std::string GetTechnologyOrder();
  void SetTechnologyOrder(const std::string &order, Error *error);
  // Set up the profile list starting with a default profile along with
  // an (optional) list of startup profiles.
  void InitializeProfiles();
  // Create a profile.  This does not affect the profile stack.  Returns
  // the RPC path of the created profile in |path|.
  void CreateProfile(const std::string &name, std::string *path, Error *error);
  // Pushes existing profile with name |name| onto stack of managed profiles.
  // Returns the RPC path of the pushed profile in |path|.
  void PushProfile(const std::string &name, std::string *path, Error *error);
  // Pops profile named |name| off the top of the stack of managed profiles.
  void PopProfile(const std::string &name, Error *error);
  // Remove the active profile.
  void PopAnyProfile(Error *error);
  // Handle the event where a profile is about to remove a profile entry.
  // Any Services that are dependent on this storage identifier will need
  // to find new profiles.  Return true if any service has been moved to a new
  // profile.  Any such services will have had the profile group removed from
  // the profile.
  virtual bool HandleProfileEntryDeletion(const ProfileRefPtr &profile,
                                          const std::string &entry_name);
  // Find a service that is both the member of |profile| and has a
  // storage identifier that matches |entry_name|.  This function is
  // called by the Profile in order to return a profile entry's properties.
  virtual ServiceRefPtr GetServiceWithStorageIdentifier(
      const ProfileRefPtr &profile,
      const std::string &entry_name,
      Error *error);


  virtual DeviceInfo *device_info() { return &device_info_; }
  ModemInfo *modem_info() { return &modem_info_; }
  PropertyStore *mutable_store() { return &store_; }
  virtual const PropertyStore &store() const { return store_; }

  std::vector<DeviceRefPtr>::iterator devices_begin() {
    return devices_.begin();
  }
  std::vector<DeviceRefPtr>::iterator devices_end() { return devices_.end(); }
  void set_startup_profiles(const std::vector<std::string> &startup_profiles) {
    startup_profiles_ = startup_profiles;
  }
  virtual const std::string &GetHostName() { return props_.host_name; }

 private:
  friend class ManagerAdaptorInterface;
  friend class ManagerTest;
  FRIEND_TEST(ManagerTest, AvailableTechnologies);
  FRIEND_TEST(ManagerTest, ConnectedTechnologies);
  FRIEND_TEST(ManagerTest, DefaultTechnology);
  FRIEND_TEST(ManagerTest, DeviceRegistrationAndStart);
  FRIEND_TEST(ManagerTest, EnumerateProfiles);
  FRIEND_TEST(ManagerTest, SortServices);
  FRIEND_TEST(ManagerTest, SortServicesWithConnection);

  static const char kManagerErrorNoDevice[];

  std::string CalculateState(Error *error);
  void AutoConnect();
  void AutoConnectTask();
  std::vector<std::string> AvailableTechnologies(Error *error);
  std::vector<std::string> ConnectedTechnologies(Error *error);
  std::string DefaultTechnology(Error *error);
  std::vector<std::string> EnabledTechnologies(Error *error);
  std::vector<std::string> EnumerateDevices(Error *error);
  std::vector<std::string> EnumerateProfiles(Error *error);
  // TODO(cmasone): This should be implemented by filtering |services_|.
  std::vector<std::string> EnumerateWatchedServices(Error *error);
  std::string GetActiveProfileRpcIdentifier(Error *error);

  void HelpRegisterDerivedString(
      const std::string &name,
      std::string(Manager::*get)(Error *),
      void(Manager::*set)(const std::string&, Error *));
  void HelpRegisterDerivedStrings(
      const std::string &name,
      Strings(Manager::*get)(Error *),
      void(Manager::*set)(const Strings&, Error *));

  void PopProfileInternal();
  bool OrderServices(ServiceRefPtr a, ServiceRefPtr b);
  void SortServices();
  bool MatchProfileWithService(const ServiceRefPtr &service);

  EventDispatcher *dispatcher_;
  ScopedRunnableMethodFactory<Manager> task_factory_;
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
  // List of startup profile names to push on the profile stack on startup.
  std::vector<std::string> startup_profiles_;
  std::vector<ProfileRefPtr> profiles_;
  ProfileRefPtr ephemeral_profile_;
  ControlInterface *control_interface_;
  Metrics *metrics_;
  GLib *glib_;

  // The priority order of technologies
  std::vector<Technology::Identifier> technology_order_;

  // Properties to be get/set via PropertyStore calls.
  Properties props_;
  PropertyStore store_;
};

}  // namespace shill

#endif  // SHILL_MANAGER_
