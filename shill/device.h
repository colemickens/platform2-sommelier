// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_DEVICE_
#define SHILL_DEVICE_

#include <string>
#include <vector>

#include <base/basictypes.h>
#include <base/memory/ref_counted.h>
#include <base/memory/scoped_ptr.h>
#include <gtest/gtest_prod.h>  // for FRIEND_TEST

#include "shill/ipconfig.h"
#include "shill/property_store.h"
#include "shill/refptr_types.h"
#include "shill/service.h"
#include "shill/shill_event.h"

namespace shill {

class ControlInterface;
class DHCPProvider;
class DeviceAdaptorInterface;
class Endpoint;
class Error;
class EventDispatcher;
class Manager;

// Device superclass.  Individual network interfaces types will inherit from
// this class.
class Device : public base::RefCounted<Device> {
 public:
  enum Technology {
    kEthernet,
    kWifi,
    kCellular,
    kBlacklisted,
    kUnknown,
    kNumTechnologies
  };

  // A constructor for the Device object
  Device(ControlInterface *control_interface,
         EventDispatcher *dispatcher,
         Manager *manager,
         const std::string &link_name,
         const std::string &address,
         int interface_index);
  virtual ~Device();

  virtual void Start();
  virtual void Stop();

  // Base method always returns false.
  virtual bool TechnologyIs(const Technology type) const;

  virtual void LinkEvent(unsigned flags, unsigned change);
  virtual void Scan();

  virtual void ConfigIP() {}

  // The default implementation sets |error| to kNotSupported.
  virtual void RegisterOnNetwork(const std::string &network_id, Error *error);

  std::string GetRpcIdentifier();
  std::string GetStorageIdentifier();

  const std::string &link_name() const { return link_name_; }
  int interface_index() const { return interface_index_; }
  const ConnectionRefPtr &connection() const { return connection_; }

  const std::string &FriendlyName() const;

  // Returns a string that is guaranteed to uniquely identify this Device
  // instance.
  const std::string &UniqueName() const;

  PropertyStore *store() { return &store_; }

  bool Load(StoreInterface *storage);
  bool Save(StoreInterface *storage);

  void set_dhcp_provider(DHCPProvider *provider) { dhcp_provider_ = provider; }

 protected:
  FRIEND_TEST(DeviceTest, AcquireDHCPConfig);
  FRIEND_TEST(DeviceTest, DestroyIPConfig);
  FRIEND_TEST(DeviceTest, DestroyIPConfigNULL);
  FRIEND_TEST(DeviceTest, GetProperties);
  FRIEND_TEST(DeviceTest, Save);
  FRIEND_TEST(DeviceTest, SelectedService);

  // If there's an IP configuration in |ipconfig_|, releases the IP address and
  // destroys the configuration instance.
  void DestroyIPConfig();

  // Creates a new DHCP IP configuration instance, stores it in |ipconfig_| and
  // requests a new IP configuration. Registers a callback to
  // IPConfigUpdatedCallback on IP configuration changes. Returns true if the IP
  // request was successfully sent.
  bool AcquireDHCPConfig();

  // Maintain connection state (Routes, IP Addresses and DNS) in the OS.
  void CreateConnection();

  // Remove connection state
  void DestroyConnection();

  // Selects a service to be "current" -- i.e. link-state or configuration
  // events that happen to the device are attributed to this service.
  void SelectService(const ServiceRefPtr &service);

  // Set the state of the selected service
  void SetServiceState(Service::ConnectState state);

  // Set the failure of the selected service (implicitly sets the state to
  // "failure")
  void SetServiceFailure(Service::ConnectFailure failure_state);

  void HelpRegisterDerivedStrings(const std::string &name,
                                  Strings(Device::*get)(void),
                                  bool(Device::*set)(const Strings&));

  // Property getters reserved for subclasses
  ControlInterface *control_interface() const { return control_interface_; }
  EventDispatcher *dispatcher() const { return dispatcher_; }
  Manager *manager() const { return manager_; }
  std::vector<ServiceRefPtr> *services() { return &services_; }

 private:
  friend class DeviceAdaptorInterface;

  static const char kStoragePowered[];
  static const char kStorageIPConfigs[];

  // Callback invoked on every IP configuration update.
  void IPConfigUpdatedCallback(const IPConfigRefPtr &ipconfig, bool success);

  // Right now, Devices reference IPConfigs directly when persisted to disk
  // It's not clear that this makes sense long-term, but that's how it is now.
  // This call generates a string in the right format for this persisting.
  // |suffix| is injected into the storage identifier used for the configs.
  std::string SerializeIPConfigs(const std::string &suffix);

  std::vector<std::string> AvailableIPConfigs();
  std::string GetRpcConnectionIdentifier();

  // Properties
  bool powered_;  // TODO(pstew): Is this what |running_| is for?
  bool reconnect_;
  const std::string hardware_address_;

  PropertyStore store_;

  std::vector<ServiceRefPtr> services_;
  const int interface_index_;
  bool running_;
  const std::string link_name_;
  const std::string unique_id_;
  ControlInterface *control_interface_;
  EventDispatcher *dispatcher_;
  Manager *manager_;
  IPConfigRefPtr ipconfig_;
  ConnectionRefPtr connection_;
  scoped_ptr<DeviceAdaptorInterface> adaptor_;

  // Maintain a reference to the connected / connecting service
  ServiceRefPtr selected_service_;

  // Cache singleton pointer for performance and test purposes.
  DHCPProvider *dhcp_provider_;

  DISALLOW_COPY_AND_ASSIGN(Device);
};

}  // namespace shill

#endif  // SHILL_DEVICE_
