// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_DEVICE_
#define SHILL_DEVICE_

#include <string>
#include <vector>

#include <base/basictypes.h>
#include <base/memory/ref_counted.h>
#include <base/memory/scoped_ptr.h>
#include <base/memory/weak_ptr.h>
#include <gtest/gtest_prod.h>  // for FRIEND_TEST

#include "shill/adaptor_interfaces.h"
#include "shill/callbacks.h"
#include "shill/event_dispatcher.h"
#include "shill/ip_address.h"
#include "shill/ipconfig.h"
#include "shill/portal_detector.h"
#include "shill/property_store.h"
#include "shill/refptr_types.h"
#include "shill/service.h"
#include "shill/technology.h"

namespace shill {

class ControlInterface;
class DHCPProvider;
class DeviceAdaptorInterface;
class Endpoint;
class Error;
class EventDispatcher;
class Manager;
class Metrics;
class RoutingTable;
class RTNLHandler;

// Device superclass.  Individual network interfaces types will inherit from
// this class.
class Device : public base::RefCounted<Device> {
 public:
  // A constructor for the Device object
  Device(ControlInterface *control_interface,
         EventDispatcher *dispatcher,
         Metrics *metrics,
         Manager *manager,
         const std::string &link_name,
         const std::string &address,
         int interface_index,
         Technology::Identifier technology);
  virtual ~Device();

  // Enable or disable the device.
  virtual void SetEnabled(bool enable);
  // Enable or disable the device, and save the setting in the profile.
  // The setting is persisted before the enable or disable operation
  // starts, so that even if it fails, the user's intent is still recorded
  // for the next time shill restarts.
  void SetEnabledPersistent(bool enable,
                            Error *error, const ResultCallback &callback);

  // Returns true if the underlying device reports that it is already enabled.
  // Used when the device is registered with the Manager, so that shill can
  // sync its state/ with the true state of the device. The default is to
  // report false.
  virtual bool IsUnderlyingDeviceEnabled() const;

  // TODO(gauravsh): We do not really need this since technology() can be used
  //                 to get a device's technology for direct comparison.
  // Base method always returns false.
  virtual bool TechnologyIs(const Technology::Identifier type) const;

  virtual void LinkEvent(unsigned flags, unsigned change);

  // The default implementation sets |error| to kNotSupported.
  virtual void Scan(Error *error);
  virtual void RegisterOnNetwork(const std::string &network_id, Error *error,
                                 const ResultCallback &callback);
  virtual void RequirePIN(const std::string &pin, bool require,
                          Error *error, const ResultCallback &callback);
  virtual void EnterPIN(const std::string &pin,
                        Error *error, const ResultCallback &callback);
  virtual void UnblockPIN(const std::string &unblock_code,
                          const std::string &pin,
                          Error *error, const ResultCallback &callback);
  virtual void ChangePIN(const std::string &old_pin,
                         const std::string &new_pin,
                         Error *error, const ResultCallback &callback);
  virtual void DisableIPv6();
  virtual void EnableIPv6();
  virtual void EnableIPv6Privacy();

  // Request the removal of reverse-path filtering for this interface.
  // This will allow packets destined for this interface to be accepted,
  // even if this is not the default route for such a packet to arrive.
  virtual void DisableReversePathFilter();

  // Request reverse-path filtering for this interface.
  virtual void EnableReversePathFilter();

  // Returns true if the selected service on the device (if any) is connected.
  // Returns false if there is no selected service, or if the selected service
  // is not connected.
  bool IsConnected() const;

  // Returns true if the selected service on the device (if any) is connected
  // and matches the passed-in argument |service|.  Returns false if there is
  // no connected service, or if it does not match |service|.
  virtual bool IsConnectedToService(const ServiceRefPtr &service) const;

  // Restart the portal detection process on a connected device.  This is
  // useful if the properties on the connected service have changed in a
  // way that may affect the decision to run portal detection at all.
  // Returns true if portal detection was started.
  virtual bool RestartPortalDetection();

  // Requests that portal detection be done, if this device has the default
  // connection.  Returns true if portal detection was started.
  virtual bool RequestPortalDetection();

  std::string GetRpcIdentifier();
  std::string GetStorageIdentifier();

  const std::string &address() const { return hardware_address_; }
  const std::string &link_name() const { return link_name_; }
  int interface_index() const { return interface_index_; }
  const ConnectionRefPtr &connection() const { return connection_; }
  bool enabled() const { return enabled_; }
  bool enabled_persistent() const { return enabled_persistent_; }
  virtual Technology::Identifier technology() const { return technology_; }
  std::string GetTechnologyString(Error *error);

  const IPConfigRefPtr &ipconfig() const { return ipconfig_; }
  void set_ipconfig(const IPConfigRefPtr &config) { ipconfig_ = config; }

  const std::string &FriendlyName() const;

  // Returns a string that is guaranteed to uniquely identify this Device
  // instance.
  const std::string &UniqueName() const;

  PropertyStore *mutable_store() { return &store_; }
  const PropertyStore &store() const { return store_; }
  RTNLHandler *rtnl_handler() { return rtnl_handler_; }

  EventDispatcher *dispatcher() const { return dispatcher_; }

  // Load configuration for the device from |storage|.  This may include
  // instantiating non-visible services for which configuration has been
  // stored.
  virtual bool Load(StoreInterface *storage);

  // Save configuration for the device to |storage|.
  virtual bool Save(StoreInterface *storage);

  void set_dhcp_provider(DHCPProvider *provider) { dhcp_provider_ = provider; }

  DeviceAdaptorInterface *adaptor() const { return adaptor_.get(); }

 protected:
  FRIEND_TEST(CellularTest, ModemStateChangeDisable);
  FRIEND_TEST(DeviceTest, AcquireIPConfig);
  FRIEND_TEST(DeviceTest, DestroyIPConfig);
  FRIEND_TEST(DeviceTest, DestroyIPConfigNULL);
  FRIEND_TEST(DeviceTest, GetProperties);
  FRIEND_TEST(DeviceTest, Save);
  FRIEND_TEST(DeviceTest, SelectedService);
  FRIEND_TEST(DeviceTest, SetServiceConnectedState);
  FRIEND_TEST(DeviceTest, Stop);
  FRIEND_TEST(DeviceTest, OnEnabledStateChanged);
  FRIEND_TEST(ManagerTest, DeviceRegistrationAndStart);
  FRIEND_TEST(ManagerTest, ConnectedTechnologies);
  FRIEND_TEST(ManagerTest, DefaultTechnology);
  FRIEND_TEST(WiFiMainTest, Connect);

  // Each device must implement this method to do the work needed to
  // enable the device to operate for establishing network connections.
  // The |error| argument, if not NULL,
  // will refer to an Error that starts out with the value
  // Error::kOperationInitiated. This reflects the assumption that
  // enable (and disable) operations will usually be non-blocking,
  // and their completion will be indicated by means of an asynchronous
  // reply sometime later. There are two circumstances in which a
  // device's Start() method may overwrite |error|:
  //
  // 1. If an early failure is detected, such that the non-blocking
  //    part of the operation never takes place, then |error| should
  //    be set to the appropriate value corresponding to the type
  //    of failure. This is the "immediate failure" case.
  // 2. If the device is enabled without performing any non-blocking
  //    steps, then |error| should be Reset, i.e., its value set
  //    to Error::kSuccess. This is the "immediate success" case.
  //
  // In these two cases, because completion is immediate, |callback|
  // is not used. If neither of these two conditions holds, then |error|
  // should not be modified, and |callback| should be passed to the
  // method that will initiate the non-blocking operation.
  virtual void Start(Error *error,
                     const EnabledStateChangedCallback &callback) = 0;

  // Each device must implement this method to do the work needed to
  // disable the device, i.e., clear any running state, and make the
  // device no longer capable of establishing network connections.
  // The discussion for Start() regarding the use of |error| and
  // |callback| apply to Stop() as well.
  virtual void Stop(Error *error,
                    const EnabledStateChangedCallback &callback) = 0;

  // The EnabledStateChangedCallback that gets passed to the device's
  // Start() and Stop() methods is bound to this method. |callback|
  // is the callback that was passed to SetEnabled().
  void OnEnabledStateChanged(const ResultCallback &callback,
                             const Error &error);

  // If there's an IP configuration in |ipconfig_|, releases the IP address and
  // destroys the configuration instance.
  void DestroyIPConfig();

  // Creates a new DHCP IP configuration instance, stores it in |ipconfig_| and
  // requests a new IP configuration. Registers a callback to
  // IPConfigUpdatedCallback on IP configuration changes. Returns true if the IP
  // request was successfully sent.
  bool AcquireIPConfig();

  // Callback invoked on every IP configuration update.
  void OnIPConfigUpdated(const IPConfigRefPtr &ipconfig, bool success);

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

  // Records the failure mode and time of the selected service, and
  // sets the Service state of the selected service to "Idle".
  // Avoids showing a failure mole in the UI.
  void SetServiceFailureSilent(Service::ConnectFailure failure_state);

  // Called by the Portal Detector whenever a trial completes.  Device
  // subclasses that choose unique mappings from portal results to connected
  // states can override this method in order to do so.
  virtual void PortalDetectorCallback(const PortalDetector::Result &result);

  // Initiate portal detection, if enabled for this device type.
  bool StartPortalDetection();

  // Stop portal detection if it is running.
  void StopPortalDetection();

  // Set the state of the selected service, with checks to make sure
  // the service is already in a connected state before doing so.
  void SetServiceConnectedState(Service::ConnectState state);

  void HelpRegisterDerivedString(
      const std::string &name,
      std::string(Device::*get)(Error *),
      void(Device::*set)(const std::string&, Error *));

  void HelpRegisterDerivedStrings(
      const std::string &name,
      Strings(Device::*get)(Error *error),
      void(Device::*set)(const Strings &value, Error *error));

  void HelpRegisterConstDerivedRpcIdentifiers(
      const std::string &name,
      RpcIdentifiers(Device::*get)(Error *));

  // Property getters reserved for subclasses
  ControlInterface *control_interface() const { return control_interface_; }
  Metrics *metrics() const { return metrics_; }
  Manager *manager() const { return manager_; }
  bool running() const { return running_; }

 private:
  friend class DeviceAdaptorInterface;
  friend class DevicePortalDetectionTest;
  friend class DeviceTest;
  friend class CellularTest;
  friend class CellularCapabilityTest;
  friend class WiFiMainTest;

  static const char kIPFlagTemplate[];
  static const char kIPFlagVersion4[];
  static const char kIPFlagVersion6[];
  static const char kIPFlagDisableIPv6[];
  static const char kIPFlagUseTempAddr[];
  static const char kIPFlagUseTempAddrUsedAndDefault[];
  static const char kIPFlagReversePathFilter[];
  static const char kIPFlagReversePathFilterEnabled[];
  static const char kIPFlagReversePathFilterLooseMode[];
  static const char kStoragePowered[];
  static const char kStorageIPConfigs[];

  void SetEnabledInternal(bool enable, bool persist,
                          Error *error, const ResultCallback &callback);

  // Right now, Devices reference IPConfigs directly when persisted to disk
  // It's not clear that this makes sense long-term, but that's how it is now.
  // This call generates a string in the right format for this persisting.
  // |suffix| is injected into the storage identifier used for the configs.
  std::string SerializeIPConfigs(const std::string &suffix);

  // Set an IP configuration flag on the device.  |ip_version| should be
  // "ipv6" or "ipv4".  |flag| should be the name of the flag to be set
  // and |value| is what this flag should be set to.
  bool SetIPFlag(IPAddress::Family family, const std::string &flag,
                 const std::string &value);

  std::vector<std::string> AvailableIPConfigs(Error *error);
  std::string GetRpcConnectionIdentifier();

  // |enabled_persistent_| is the value of the Powered property, as
  // read from the profile. If it is not found in the profile, it
  // defaults to true. |enabled_| reflects the real-time state of
  // the device, i.e., enabled or disabled. |enabled_pending_| reflects
  // the target state of the device while an enable or disable operation
  // is occurring.
  //
  // Some typical sequences for these state variables are shown below.
  //
  // Shill starts up, profile has been read:
  //  |enabled_persistent_|=true   |enabled_|=false   |enabled_pending_|=false
  //
  // Shill acts on the value of |enabled_persistent_|, calls SetEnabled(true):
  //  |enabled_persistent_|=true   |enabled_|=false   |enabled_pending_|=true
  //
  // SetEnabled completes successfully, device is enabled:
  //  |enabled_persistent_|=true   |enabled_|=true    |enabled_pending_|=true
  //
  // User presses "Disable" button, SetEnabled(false) is called:
  //  |enabled_persistent_|=false   |enabled_|=true    |enabled_pending_|=false
  //
  // SetEnabled completes successfully, device is disabled:
  //  |enabled_persistent_|=false   |enabled_|=false    |enabled_pending_|=false
  bool enabled_;
  bool enabled_persistent_;
  bool enabled_pending_;

  // Other properties
  bool reconnect_;
  const std::string hardware_address_;

  PropertyStore store_;

  const int interface_index_;
  bool running_;  // indicates whether the device is actually in operation
  const std::string link_name_;
  const std::string unique_id_;
  ControlInterface *control_interface_;
  EventDispatcher *dispatcher_;
  Metrics *metrics_;
  Manager *manager_;
  IPConfigRefPtr ipconfig_;
  ConnectionRefPtr connection_;
  base::WeakPtrFactory<Device> weak_ptr_factory_;
  scoped_ptr<DeviceAdaptorInterface> adaptor_;
  scoped_ptr<PortalDetector> portal_detector_;
  base::Callback<void(const PortalDetector::Result &)>
      portal_detector_callback_;
  Technology::Identifier technology_;
  // The number of portal detection attempts from Connected to Online state.
  // This includes all failure/timeout attempts and the final successful
  // attempt.
  int portal_attempts_to_online_;

  // Maintain a reference to the connected / connecting service
  ServiceRefPtr selected_service_;

  // Cache singleton pointers for performance and test purposes.
  DHCPProvider *dhcp_provider_;
  RoutingTable *routing_table_;
  RTNLHandler *rtnl_handler_;

  DISALLOW_COPY_AND_ASSIGN(Device);
};

}  // namespace shill

#endif  // SHILL_DEVICE_
