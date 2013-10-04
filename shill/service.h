// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_SERVICE_H_
#define SHILL_SERVICE_H_

#include <time.h>

#include <deque>
#include <map>
#include <set>
#include <string>
#include <vector>

#include <base/cancelable_callback.h>
#include <base/memory/ref_counted.h>
#include <base/memory/scoped_ptr.h>
#include <base/memory/weak_ptr.h>
#include <gtest/gtest_prod.h>  // for FRIEND_TEST

#include "shill/adaptor_interfaces.h"
#include "shill/accessor_interface.h"
#include "shill/callbacks.h"
#include "shill/property_store.h"
#include "shill/refptr_types.h"
#include "shill/shill_time.h"
#include "shill/static_ip_parameters.h"
#include "shill/technology.h"

namespace chromeos_metrics {
class Timer;
}

namespace shill {

class ControlInterface;
class DiagnosticsReporter;
class EapCredentials;
class Endpoint;
class Error;
class EventDispatcher;
class HTTPProxy;
class KeyValueStore;
class Manager;
class Metrics;
class MockManager;
class ServiceAdaptorInterface;
class ServiceMockAdaptor;
class Sockets;
class StoreInterface;

// A Service is a uniquely named entity, which the system can
// connect in order to begin sending and receiving network traffic.
// All Services are bound to an Entry, which represents the persistable
// state of the Service.  If the Entry is populated at the time of Service
// creation, that information is used to prime the Service.  If not, the Entry
// becomes populated over time.
class Service : public base::RefCounted<Service> {
 public:
  static const char kCheckPortalAuto[];
  static const char kCheckPortalFalse[];
  static const char kCheckPortalTrue[];

  static const char kErrorDetailsNone[];

  // TODO(pstew): Storage constants shouldn't need to be public
  // crbug.com/208736
  static const char kStorageAutoConnect[];
  static const char kStorageCheckPortal[];
  static const char kStorageError[];
  static const char kStorageFavorite[];
  static const char kStorageGUID[];
  static const char kStorageHasEverConnected[];
  static const char kStorageLastDHCPOptionFailure[];
  static const char kStorageName[];
  static const char kStoragePriority[];
  static const char kStorageProxyConfig[];
  static const char kStorageSaveCredentials[];
  static const char kStorageType[];
  static const char kStorageUIData[];

  static const uint8 kStrengthMax;
  static const uint8 kStrengthMin;

  enum ConnectFailure {
    kFailureUnknown,
    kFailureAAA,
    kFailureActivation,
    kFailureBadPassphrase,
    kFailureBadWEPKey,
    kFailureConnect,
    kFailureDHCP,
    kFailureDNSLookup,
    kFailureEAPAuthentication,
    kFailureEAPLocalTLS,
    kFailureEAPRemoteTLS,
    kFailureHTTPGet,
    kFailureIPSecCertAuth,
    kFailureIPSecPSKAuth,
    kFailureInternal,
    kFailureNeedEVDO,
    kFailureNeedHomeNetwork,
    kFailureOTASP,
    kFailureOutOfRange,
    kFailurePPPAuth,
    kFailurePinMissing,
    kFailureMax
  };
  enum ConnectState {
    kStateUnknown,
    kStateIdle,
    kStateAssociating,
    kStateConfiguring,
    kStateConnected,
    kStatePortal,
    kStateFailure,
    kStateOnline
  };
  enum CryptoAlgorithm {
    kCryptoNone,
    kCryptoRc4,
    kCryptoAes
  };
  enum DHCPOptionFailureState {
    kDHCPOptionFailureNotDetected,
    kDHCPOptionFailureSuspected,
    kDHCPOptionFailureConfirmed,
    kDHCPOptionFailureRetestFullRequest,
    kDHCPOptionFailureRetestMinimalRequest,
    kDHCPOptionFailureRetestGotNoReply
  };
  static const int kPriorityNone;

  // A constructor for the Service object
  Service(ControlInterface *control_interface,
          EventDispatcher *dispatcher,
          Metrics *metrics,
          Manager *manager,
          Technology::Identifier technology);

  // AutoConnect MAY choose to ignore the connection request in some
  // cases. For example, if the corresponding Device only supports one
  // concurrent connection, and another Service is already connected
  // or connecting.
  //
  // AutoConnect MAY issue RPCs immediately. So AutoConnect MUST NOT
  // be called from a D-Bus signal handler context.
  virtual void AutoConnect();
  // Queue up a connection attempt. Derived classes SHOULD call the
  // base class implementation before beginning a connect. The base
  // class will log the connection attempt, and update base-class
  // state.
  virtual void Connect(Error *error, const char *reason);
  // Disconnect this service.  Override this method to add your service specific
  // disconnect logic, but call the super class's Disconnect() first.
  virtual void Disconnect(Error *error);
  // Disconnects this service via Disconnect().  Marks the service as having
  // failed with |failure|.  Do not override this method.
  virtual void DisconnectWithFailure(ConnectFailure failure, Error *error);
  // Disconnects this service via Disconnect(). The service will not be eligible
  // for auto-connect until a subsequent call to Connect, or Load.  Do not
  // override this method.
  virtual void UserInitiatedDisconnect(Error *error);

  // The default implementation returns the error kInvalidArguments.
  virtual void ActivateCellularModem(const std::string &carrier,
                                     Error *error,
                                     const ResultCallback &callback);
  // The default implementation returns the error kNotSupported.
  virtual void CompleteCellularActivation(Error *error);

  virtual bool IsActive(Error *error);

  // Returns whether services of this type should be auto-connect by default.
  virtual bool IsAutoConnectByDefault() const { return false; }

  virtual ConnectState state() const { return state_; }
  // Updates the state of the Service and alerts the manager.  Also
  // clears |failure_| if the new state isn't a failure.
  virtual void SetState(ConnectState state);
  std::string GetStateString() const;

  // State utility functions
  static bool IsConnectedState(ConnectState state);
  static bool IsConnectingState(ConnectState state);

  virtual bool IsConnected() const;
  virtual bool IsConnecting() const;
  virtual bool IsFailed() const {
    // We sometimes lie about the failure state, to keep Chrome happy
    // (see comment in WiFi::HandleDisconnect). Hence, we check both
    // state and |failed_time_|.
    return state() == kStateFailure || failed_time_ > 0;
  }

  // Returns true if the connection for |this| depends on service |b|.
  virtual bool IsDependentOn(const ServiceRefPtr &b) const;

  virtual bool IsPortalled() const {
    return state() == kStatePortal;
  }

  virtual ConnectFailure failure() const { return failure_; }
  // Records the failure mode and time. Sets the Service state to "Failure".
  virtual void SetFailure(ConnectFailure failure);
  // Records the failure mode and time. Sets the Service state to "Idle".
  // Avoids showing a failure mole in the UI.
  virtual void SetFailureSilent(ConnectFailure failure);

  // Returns a string that is guaranteed to uniquely identify this Service
  // instance.
  const std::string &unique_name() const { return unique_name_; }

  virtual std::string GetRpcIdentifier() const;

  // Returns the unique persistent storage identifier for the service.
  virtual std::string GetStorageIdentifier() const = 0;

  // Returns the identifier within |storage| from which configuration for
  // this service can be loaded.  Returns an empty string if no entry in
  // |storage| can be used.
  virtual std::string GetLoadableStorageIdentifier(
      const StoreInterface &storage) const;

  // Returns whether the service configuration can be loaded from |storage|.
  virtual bool IsLoadableFrom(const StoreInterface &storage) const;

  // Returns true if the service uses 802.1x for key management.
  virtual bool Is8021x() const { return false; }

  // Loads the service from persistent |storage|. Returns true on success.
  virtual bool Load(StoreInterface *storage);

  // Indicate to service that it is no longer persisted to storage.  It
  // should purge any stored profile state (e.g., credentials).  Returns
  // true to indicate that this service should also be unregistered from
  // the manager, false otherwise.
  virtual bool Unload();

  // Attempt to remove the service. On failure, no changes in state will occur.
  virtual void Remove(Error *error);

  // Saves the service to persistent |storage|. Returns true on success.
  virtual bool Save(StoreInterface *storage);

  // Saves the service to the current profile.
  virtual void SaveToCurrentProfile();

  // Applies all the properties in |args| to this service object's mutable
  // store, except for those in parameters_ignored_for_configure_.
  // Returns an error in |error| if one or more parameter set attempts
  // fails, but will only return the first error.
  virtual void Configure(const KeyValueStore &args, Error *error);

  // Iterate over all the properties in |args| and test for an identical
  // value in this service object's store.  Returns false if one or more
  // keys in |args| do not exist or have different values, true otherwise.
  virtual bool DoPropertiesMatch(const KeyValueStore &args) const;

  // Returns whether portal detection is explicitly disabled on this service
  // via a property set on it.
  virtual bool IsPortalDetectionDisabled() const;

  // Returns whether portal detection is set to follow the default setting
  // of this service's technology via a property set on it.
  virtual bool IsPortalDetectionAuto() const;

  // Returns true if the service is persisted to a non-ephemeral profile.
  virtual bool IsRemembered() const;

  // Returns true if the service RPC identifier should be part of the
  // manager's advertised services list, false otherwise.
  virtual bool IsVisible() const { return true; }

  // Returns true if there is a proxy configuration set on this service.
  virtual bool HasProxyConfig() const { return !proxy_config_.empty(); }

  // Returns whether this service has had recent connection issues.
  virtual bool HasRecentConnectionIssues();

  // If the AutoConnect property has not already been marked as saved, set
  // its value to true and mark it saved.
  virtual void EnableAndRetainAutoConnect();

  // Set the connection for this service.  If the connection is non-NULL, create
  // an HTTP Proxy that will utilize this service's connection to serve
  // requests.
  virtual void SetConnection(const ConnectionRefPtr &connection);
  virtual const ConnectionRefPtr &connection() const { return connection_; }

  // Examines the EAP credentials for the service and returns true if a
  // connection attempt can be made.
  virtual bool Is8021xConnectable() const;

  // Add an EAP certification id |name| at position |depth| in the stack.
  // Returns true if entry was added, false otherwise.
  virtual bool AddEAPCertification(const std::string &name, size_t depth);
  // Clear all EAP certification elements.
  virtual void ClearEAPCertification();

  // The inherited class that needs to send metrics after the service has
  // transitioned to the ready state should override this method.
  // |time_resume_to_ready_milliseconds| holds the elapsed time from when
  // the system was resumed until when the service transitioned to the
  // connected state.  This value is non-zero for the first service transition
  // to the connected state after a resume.
  virtual void SendPostReadyStateMetrics(
      int64 /*time_resume_to_ready_milliseconds*/) const {}

  bool auto_connect() const { return auto_connect_; }
  void SetAutoConnect(bool connect);

  bool connectable() const { return connectable_; }
  // Sets the connectable property of the service, and broadcast the
  // new value. Does not update the manager.
  // TODO(petkov): Remove this method in favor of SetConnectableFull.
  void SetConnectable(bool connectable);
  // Sets the connectable property of the service, broadcasts the new
  // value, and alerts the manager if necessary.
  void SetConnectableFull(bool connectable);

  virtual bool explicitly_disconnected() const {
    return explicitly_disconnected_;
  }

  bool retain_auto_connect() const { return retain_auto_connect_; }
  // Setter is deliberately omitted; use EnableAndRetainAutoConnect.

  void set_friendly_name(const std::string &n) { friendly_name_ = n; }
  const std::string &friendly_name() const { return friendly_name_; }
  // Sets the kNameProperty and broadcasts the change.
  void SetFriendlyName(const std::string &friendly_name);

  const std::string &guid() const { return guid_; }
  bool SetGuid(const std::string &guid, Error *error);

  bool has_ever_connected() const { return has_ever_connected_; }

  int32 priority() const { return priority_; }
  bool SetPriority(const int32 &priority, Error *error);

  size_t crypto_algorithm() const { return crypto_algorithm_; }
  bool key_rotation() const { return key_rotation_; }
  bool endpoint_auth() const { return endpoint_auth_; }

  void SetStrength(uint8 strength);

  // uint8 streams out as a char. Coerce to a larger type, so that
  // it prints as a number.
  uint16 strength() const { return strength_; }

  virtual Technology::Identifier technology() const { return technology_; }
  std::string GetTechnologyString() const;

  virtual const EapCredentials *eap() const { return eap_.get(); }
  void SetEapCredentials(EapCredentials *eap);

  bool save_credentials() const { return save_credentials_; }
  void set_save_credentials(bool save) { save_credentials_ = save; }

  const std::string &error() const { return error_; }
  void set_error(const std::string &error) { error_ = error; }

  const std::string &error_details() const { return error_details_; }
  void SetErrorDetails(const std::string &details);

  static const char *ConnectFailureToString(const ConnectFailure &state);
  static const char *ConnectStateToString(const ConnectState &state);

  // Compare two services.  Returns true if Service |a| should be displayed
  // above |b|.  If |compare_connectivity_state| is true, the connectivity
  // state of the service (service->state()) is used as the most significant
  // criteria for comparsion, otherwise the service state is ignored.  Use
  // |tech_order| to rank services if more decisive criteria do not yield a
  // difference.  |reason| is populated with the exact criteria used for the
  // ultimate comparison.
  static bool Compare(ServiceRefPtr a,
                      ServiceRefPtr b,
                      bool compare_connectivity_state,
                      const std::vector<Technology::Identifier> &tech_order,
                      const char **reason);

  // These are defined in service.cc so that we don't have to include profile.h
  // TODO(cmasone): right now, these are here only so that we can get the
  // profile name as a property.  Can we store just the name, and then handle
  // setting the profile for this service via |manager_|?
  const ProfileRefPtr &profile() const;

  // Sets the profile property of this service. Broadcasts the new value if it's
  // not NULL. If the new value is NULL, the service will either be set to
  // another profile afterwards or it will not be visible and not monitored
  // anymore.
  void SetProfile(const ProfileRefPtr &p);

  // This is called from tests and shouldn't be called otherwise. Use SetProfile
  // instead.
  void set_profile(const ProfileRefPtr &p);

  // Notification that occurs when a service now has profile data saved
  // on its behalf.  Some service types like WiFi can choose to register
  // themselves at this point.
  virtual void OnProfileConfigured() {}

  // Notification that occurs when a single property has been changed via
  // the RPC adaptor.
  virtual void OnPropertyChanged(const std::string &property);

  // Notification that occurs when an EAP credential property has been
  // changed.  Some service subclasses can choose to respond to this
  // event.
  virtual void OnEapCredentialsChanged() {}

  // Called by the manager once after a resume.
  virtual void OnAfterResume();

  // Called by the device if a DHCP attempt fails.
  virtual void OnDHCPFailure();

  // Called by the device if a DHCP attempt succeeds.
  virtual void OnDHCPSuccess();

  // Called by the device to test for historical DHCP issues.
  virtual bool ShouldUseMinimalDHCPConfig();

  EapCredentials *mutable_eap() { return eap_.get(); }

  PropertyStore *mutable_store() { return &store_; }
  const PropertyStore &store() const { return store_; }
  StaticIPParameters *mutable_static_ip_parameters() {
    return &static_ip_parameters_;
  }
  const StaticIPParameters &static_ip_parameters() const {
    return static_ip_parameters_;
  }

  // Retrieves |key| from |id| in |storage| to |value|.  If this key does
  // not exist, assign |default_value| to |value|.
  static void LoadString(StoreInterface *storage,
                         const std::string &id,
                         const std::string &key,
                         const std::string &default_value,
                         std::string *value);

  // Assigns |value| to |key| in |storage| if |value| is non-empty and |save| is
  // true. Otherwise, removes |key| from |storage|. If |crypted| is true, the
  // value is encrypted.
  static void SaveString(StoreInterface *storage,
                         const std::string &id,
                         const std::string &key,
                         const std::string &value,
                         bool crypted,
                         bool save);

  // Called via RPC to get a dict containing profile-to-entry_name mappings
  // of all the profile entires which contain configuration applicable to
  // this service.
  std::map<std::string, std::string> GetLoadableProfileEntries();

 protected:
  friend class base::RefCounted<Service>;

  static const char kAutoConnBusy[];

  virtual ~Service();

  // Returns true if a character is allowed to be in a service storage id.
  static bool LegalChar(char a) { return isalnum(a) || a == '_'; }

  // Returns true if a character is disallowed to be in a service storage id.
  static bool IllegalChar(char a) { return !LegalChar(a); }

  virtual std::string CalculateState(Error *error);
  std::string CalculateTechnology(Error *error);

  // Returns whether this service is in a state conducive to auto-connect.
  // This should include any tests used for computing connectable(),
  // as well as other critera such as whether the device associated with
  // this service is busy with another connection.
  //
  // If the service is not auto-connectable, |*reason| will be set to
  // point to C-string explaining why the service is not auto-connectable.
  virtual bool IsAutoConnectable(const char **reason) const;

  // HelpRegisterDerived*: Expose a property over RPC, with the name |name|.
  //
  // Reads of the property will be handled by invoking |get|.
  // Writes to the property will be handled by invoking |set|.
  // Clearing the property will be handled by PropertyStore.
  void HelpRegisterDerivedBool(
      const std::string &name,
      bool(Service::*get)(Error *error),
      bool(Service::*set)(const bool &value, Error *error),
      void(Service::*clear)(Error *error));
  void HelpRegisterDerivedInt32(
      const std::string &name,
      int32(Service::*get)(Error *error),
      bool(Service::*set)(const int32 &value, Error *error));
  void HelpRegisterDerivedString(
      const std::string &name,
      std::string(Service::*get)(Error *error),
      bool(Service::*set)(const std::string &value, Error *error));
  void HelpRegisterConstDerivedUint16(
      const std::string &name,
      uint16(Service::*get)(Error *error));
  void HelpRegisterConstDerivedRpcIdentifier(
      const std::string &name,
      std::string(Service::*get)(Error *));
  void HelpRegisterConstDerivedStrings(
      const std::string &name, Strings(Service::*get)(Error *error));

  ServiceAdaptorInterface *adaptor() const { return adaptor_.get(); }

  void UnloadEapCredentials();

  // Ignore |parameter| when performing a Configure() operation.
  void IgnoreParameterForConfigure(const std::string &parameter);

  // Update the service's string-based "Error" RPC property based on the
  // failure_ enum.
  void UpdateErrorProperty();

  // RPC setter for the the "AutoConnect" property. Updates the |manager_|.
  // (cf. SetAutoConnect, which does not update the manager.)
  virtual bool SetAutoConnectFull(const bool &connect, Error *error);

  // RPC clear method for the "AutoConnect" property.  Sets the AutoConnect
  // property back to its default value, and clears the retain_auto_connect_
  // property to allow the AutoConnect property to be enabled automatically.
  void ClearAutoConnect(Error *error);

  // Property accessors reserved for subclasses
  EventDispatcher *dispatcher() const { return dispatcher_; }
  const std::string &GetEAPKeyManagement() const;
  virtual void SetEAPKeyManagement(const std::string &key_management);
  Manager *manager() const { return manager_; }
  Metrics *metrics() const { return metrics_; }

  // Save the serivce's auto_connect value, without affecting its auto_connect
  // property itself. (cf. EnableAndRetainAutoConnect)
  void RetainAutoConnect();

  // Inform base class of the security properties for the service.
  //
  // NB: When adding a call to this function from a subclass, please check
  // that the semantics of SecurityLevel() are appropriate for the subclass.
  void SetSecurity(CryptoAlgorithm crypt, bool rotation, bool endpoint_auth);

 private:
  friend class ActivePassiveOutOfCreditsDetectorTest;
  friend class EthernetEapServiceTest;
  friend class EthernetServiceTest;
  friend class MetricsTest;
  friend class ManagerTest;
  friend class ServiceAdaptorInterface;
  friend class ServiceTest;
  friend class SubscriptionStateOutOfCreditsDetectorTest;
  friend class VPNProviderTest;
  friend class VPNServiceTest;
  friend class WiFiServiceTest;
  friend class WiMaxProviderTest;
  friend class WiMaxServiceTest;
  friend void TestCommonPropertyChanges(ServiceRefPtr, ServiceMockAdaptor *);
  friend void TestCustomSetterNoopChange(ServiceRefPtr, MockManager *);
  friend void TestNamePropertyChange(ServiceRefPtr, ServiceMockAdaptor *);
  FRIEND_TEST(AllMockServiceTest, AutoConnectWithFailures);
  FRIEND_TEST(CellularCapabilityGSMTest, SetStorageIdentifier);
  FRIEND_TEST(CellularCapabilityUniversalCDMAMainTest,
              UpdateStorageIdentifier);
  FRIEND_TEST(CellularCapabilityUniversalMainTest, UpdateStorageIdentifier);
  FRIEND_TEST(CellularServiceTest, IsAutoConnectable);
  FRIEND_TEST(DeviceTest, IPConfigUpdatedFailureWithStatic);
  FRIEND_TEST(ManagerTest, ConnectToBestServices);
  // TODO(quiche): The SortServices test should probably move into
  // service_unittest.cc. Then the following line should be removed.
  // (crbug.com/206367)
  FRIEND_TEST(ManagerTest, SortServices);
  FRIEND_TEST(ServiceTest, AutoConnectLogging);
  FRIEND_TEST(ServiceTest, CalculateState);
  FRIEND_TEST(ServiceTest, CalculateTechnology);
  FRIEND_TEST(ServiceTest, Certification);
  FRIEND_TEST(ServiceTest, ConfigureEapStringProperty);
  FRIEND_TEST(ServiceTest, ConfigureIgnoredProperty);
  FRIEND_TEST(ServiceTest, Constructor);
  FRIEND_TEST(ServiceTest, CustomSetterNoopChange);
  FRIEND_TEST(ServiceTest, GetIPConfigRpcIdentifier);
  FRIEND_TEST(ServiceTest, GetProperties);
  FRIEND_TEST(ServiceTest, IsAutoConnectable);
  FRIEND_TEST(ServiceTest, IsDependentOn);
  FRIEND_TEST(ServiceTest, Load);
  FRIEND_TEST(ServiceTest, RecheckPortal);
  FRIEND_TEST(ServiceTest, Save);
  FRIEND_TEST(ServiceTest, SaveString);
  FRIEND_TEST(ServiceTest, SaveStringCrypted);
  FRIEND_TEST(ServiceTest, SaveStringDontSave);
  FRIEND_TEST(ServiceTest, SaveStringEmpty);
  FRIEND_TEST(ServiceTest, SecurityLevel);
  FRIEND_TEST(ServiceTest, SetCheckPortal);
  FRIEND_TEST(ServiceTest, SetConnectableFull);
  FRIEND_TEST(ServiceTest, SetFriendlyName);
  FRIEND_TEST(ServiceTest, SetProperty);
  FRIEND_TEST(ServiceTest, State);
  FRIEND_TEST(ServiceTest, StateResetAfterFailure);
  FRIEND_TEST(ServiceTest, UniqueAttributes);
  FRIEND_TEST(ServiceTest, Unload);
  FRIEND_TEST(WiFiServiceTest, SuspectedCredentialFailure);
  FRIEND_TEST(WiFiTimerTest, ReconnectTimer);
  FRIEND_TEST(WiFiMainTest, EAPEvent);  // For eap_.

  static const char kAutoConnConnected[];
  static const char kAutoConnConnecting[];
  static const char kAutoConnExplicitDisconnect[];
  static const char kAutoConnNotConnectable[];
  static const char kAutoConnOffline[];
  static const char kAutoConnThrottled[];

  static const size_t kEAPMaxCertificationElements;

  static const char kServiceSortAutoConnect[];
  static const char kServiceSortConnectable[];
  static const char kServiceSortHasEverConnected[];
  static const char kServiceSortIsConnected[];
  static const char kServiceSortDependency[];
  static const char kServiceSortIsConnecting[];
  static const char kServiceSortIsFailed[];
  static const char kServiceSortIsPortalled[];
  static const char kServiceSortPriority[];
  static const char kServiceSortSecurityEtc[];
  static const char kServiceSortSerialNumber[];
  static const char kServiceSortTechnology[];

  static const uint64 kMaxAutoConnectCooldownTimeMilliseconds;
  static const uint64 kMinAutoConnectCooldownTimeMilliseconds;
  static const uint64 kAutoConnectCooldownBackoffFactor;

  static const int kDisconnectsMonitorSeconds;
  static const int kMisconnectsMonitorSeconds;
  static const int kReportDisconnectsThreshold;
  static const int kReportMisconnectsThreshold;
  static const int kMaxDisconnectEventHistory;

  // The number of DHCP failures to allow before we start suspecting that
  // this may be indirectly due to the number of options we are requesting.
  static const int kMaxDHCPOptionFailures;

  // The number of DHCP failures to allow before we start suspecting that
  // this may be indirectly due to the number of options we are requesting.
  static const int kDHCPOptionHoldOffPeriodSeconds;


  bool GetAutoConnect(Error *error);

  std::string GetCheckPortal(Error *error);
  bool SetCheckPortal(const std::string &check_portal, Error *error);

  std::string GetGuid(Error *error);

  virtual std::string GetDeviceRpcId(Error *error) = 0;

  std::string GetIPConfigRpcIdentifier(Error *error);

  std::string GetNameProperty(Error *error);
  // The base implementation asserts that |name| matches the current Name
  // property value.
  virtual bool SetNameProperty(const std::string &name, Error *error);

  int32 GetPriority(Error *error);

  std::string GetProfileRpcId(Error *error);
  bool SetProfileRpcId(const std::string &profile, Error *error);

  // Returns TCP port of service's HTTP proxy in host order.
  uint16 GetHTTPProxyPort(Error *error);

  std::string GetProxyConfig(Error *error);
  bool SetProxyConfig(const std::string &proxy_config, Error *error);

  static Strings ExtractWallClockToStrings(
      const std::deque<Timestamp> &timestamps);
  Strings GetDisconnectsProperty(Error *error);
  Strings GetMisconnectsProperty(Error *error);

  void ReEnableAutoConnectTask();
  // Disables autoconnect and posts a task to re-enable it after a cooldown.
  // Note that autoconnect could be disabled for other reasons as well.
  void ThrottleFutureAutoConnects();

  // Saves settings to profile, if we have one. Unlike
  // SaveServiceToProfile, SaveToProfile never assigns this service
  // into a profile.
  void SaveToProfile();

  // Start at the head of |events| and remove all entries that occurred
  // more than |seconds_ago| prior to |now|.  Also, the size of |events|
  // is unconditionally trimmed below kMaxDisconnectEventHistory.
  static void ExpireEventsBefore(
      int seconds_ago, const Timestamp &now, std::deque<Timestamp> *events);

  // Qualify the conditions under which the most recent disconnect occurred.
  // Make note ot the fact that there was a problem connecting / staying
  // connected if the disconnection did not occur as a clear result of user
  // action.
  void NoteDisconnectEvent();

  // Utility function that returns true if a is different from b.  When they
  // are, "decision" is populated with the boolean value of "a > b".
  static bool DecideBetween(int a, int b, bool *decision);

  // Linearize security parameters (crypto algorithm, key rotation, endpoint
  // authentication) for comparison.
  uint16 SecurityLevel();

  // WeakPtrFactory comes first, so that other fields can use it.
  base::WeakPtrFactory<Service> weak_ptr_factory_;

  ConnectState state_;
  ConnectState previous_state_;
  ConnectFailure failure_;
  bool auto_connect_;

  // Denotes whether the value of auto_connect_ property value should be
  // retained, i.e. only be allowed to change via explicit property changes
  // from the UI.
  bool retain_auto_connect_;

  std::string check_portal_;
  bool connectable_;
  std::string error_;
  std::string error_details_;
  bool explicitly_disconnected_;
  int32 priority_;
  uint8 crypto_algorithm_;
  bool key_rotation_;
  bool endpoint_auth_;

  uint8 strength_;
  std::string proxy_config_;
  std::string ui_data_;
  std::string guid_;
  bool save_credentials_;
  scoped_ptr<EapCredentials> eap_;
  Technology::Identifier technology_;
  // The time of the most recent failure. Value is 0 if the service is
  // not currently failed.
  time_t failed_time_;
  // Whether or not this service has ever reached kStateConnected.
  bool has_ever_connected_;

  std::deque<Timestamp> disconnects_;  // Connection drops.
  std::deque<Timestamp> misconnects_;  // Failures to connect.

  base::CancelableClosure reenable_auto_connect_task_;
  uint64 auto_connect_cooldown_milliseconds_ ;

  ProfileRefPtr profile_;
  PropertyStore store_;
  std::set<std::string> parameters_ignored_for_configure_;

  EventDispatcher *dispatcher_;
  unsigned int serial_number_;
  std::string unique_name_;  // MUST be unique amongst service instances

  // Service's friendly name is presented through the UI. By default it's the
  // same as |unique_name_| but normally Service subclasses override
  // it. WARNING: Don't log the friendly name at the default logging level due
  // to PII concerns.
  std::string friendly_name_;

  // List of subject names reported by remote entity during TLS setup.
  std::vector<std::string> remote_certification_;

  scoped_ptr<ServiceAdaptorInterface> adaptor_;
  scoped_ptr<HTTPProxy> http_proxy_;
  ConnectionRefPtr connection_;
  StaticIPParameters static_ip_parameters_;
  Metrics *metrics_;
  Manager *manager_;
  scoped_ptr<Sockets> sockets_;
  Time *time_;
  DiagnosticsReporter *diagnostics_reporter_;

  // Tracks the number of consecutive failed DHCP attempts.
  int consecutive_dhcp_failures_;

  // Tracks the last time we had a failure that appered correlated
  // to the DHCP client asking for too many options.
  Timestamp last_dhcp_option_failure_;

  // Tracks the current progress in deciding if DHCP failures appear
  // correlated to requests for a large number of DHCP options.
  DHCPOptionFailureState dhcp_option_failure_state_;

  // The |serial_number_| for the next Service.
  static unsigned int next_serial_number_;

  DISALLOW_COPY_AND_ASSIGN(Service);
};

}  // namespace shill

#endif  // SHILL_SERVICE_H_
