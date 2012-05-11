// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_SERVICE_
#define SHILL_SERVICE_

#include <time.h>

#include <string>
#include <map>
#include <set>
#include <vector>

#include <base/memory/ref_counted.h>
#include <base/memory/scoped_ptr.h>
#include <gtest/gtest_prod.h>  // for FRIEND_TEST

#include "shill/adaptor_interfaces.h"
#include "shill/accessor_interface.h"
#include "shill/callbacks.h"
#include "shill/property_store.h"
#include "shill/refptr_types.h"
#include "shill/static_ip_parameters.h"
#include "shill/technology.h"

namespace chromeos_metrics {
class Timer;
}

namespace shill {

class Configuration;
class ControlInterface;
class Endpoint;
class Error;
class EventDispatcher;
class HTTPProxy;
class KeyValueStore;
class Manager;
class Metrics;
class ServiceAdaptorInterface;
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

  // TODO(pstew): Storage constants shouldn't need to be public
  // crosbug.com/25813
  static const char kStorageAutoConnect[];
  static const char kStorageCheckPortal[];
  static const char kStorageEapAnonymousIdentity[];
  static const char kStorageEapCACert[];
  static const char kStorageEapCACertID[];
  static const char kStorageEapCACertNSS[];
  static const char kStorageEapCertID[];
  static const char kStorageEapClientCert[];
  static const char kStorageEapEap[];
  static const char kStorageEapIdentity[];
  static const char kStorageEapInnerEap[];
  static const char kStorageEapKeyID[];
  static const char kStorageEapKeyManagement[];
  static const char kStorageEapPIN[];
  static const char kStorageEapPassword[];
  static const char kStorageEapPrivateKey[];
  static const char kStorageEapPrivateKeyPassword[];
  static const char kStorageEapUseSystemCAs[];
  static const char kStorageError[];
  static const char kStorageFavorite[];
  static const char kStorageGUID[];
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
    kFailureActivationFailure,
    kFailureOutOfRange,
    kFailurePinMissing,
    kFailureConfigurationFailed,
    kFailureBadCredentials,
    kFailureNeedEVDO,
    kFailureNeedHomeNetwork,
    kFailureOTASPFailure,
    kFailureAAAFailure,
    kFailureMax
  };
  enum ConnectState {
    kStateUnknown,
    kStateIdle,
    kStateAssociating,
    kStateConfiguring,
    kStateConnected,
    kStateDisconnected,
    kStatePortal,
    kStateFailure,
    kStateOnline
  };
  struct EapCredentials {
    EapCredentials() : use_system_cas(true) {}
    std::string identity;
    std::string eap;
    std::string inner_eap;
    std::string anonymous_identity;
    std::string client_cert;
    std::string cert_id;
    std::string private_key;
    std::string private_key_password;
    std::string key_id;
    std::string ca_cert;
    std::string ca_cert_id;
    std::string ca_cert_nss;
    bool use_system_cas;
    std::string pin;
    std::string password;
    std::string key_management;
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
  // Queue up a connection attempt.
  virtual void Connect(Error *error);
  // Disconnect this service. The service will not be eligible for
  // auto-connect until a subsequent call to Connect, or Load.
  virtual void Disconnect(Error *error);

  // The default implementation returns the error kInvalidArguments.
  virtual void ActivateCellularModem(const std::string &carrier,
                                     Error *error,
                                     const ResultCallback &callback);

  // Base method always returns false.
  virtual bool TechnologyIs(const Technology::Identifier type) const;

  virtual bool IsActive(Error *error);

  virtual ConnectState state() const { return state_; }
  // Updates the state of the Service and alerts the manager.  Also
  // clears |failure_| if the new state isn't a failure.
  virtual void SetState(ConnectState state);

  // State utility functions
  virtual bool IsConnected() const {
    return state() == kStateConnected || state() == kStatePortal ||
        state() == kStateOnline;
  }
  virtual bool IsConnecting() const {
    return state() == kStateAssociating || state() == kStateConfiguring;
  }
  virtual bool IsFailed() const {
    // We sometimes lie about the failure state, to keep Chrome happy
    // (see comment in WiFi::HandleDisconnect). Hence, we check both
    // state and |failed_time_|.
    return state() == kStateFailure || failed_time_ > 0;
  }

  virtual ConnectFailure failure() const { return failure_; }
  // Records the failure mode and time. Sets the Service state to "Failure".
  virtual void SetFailure(ConnectFailure failure);
  // Records the failure mode and time. Sets the Service state to "Idle".
  // Avoids showing a failure mole in the UI.
  virtual void SetFailureSilent(ConnectFailure failure);

  // Returns a string that is guaranteed to uniquely identify this Service
  // instance.
  const std::string &UniqueName() const { return unique_name_; }

  virtual std::string GetRpcIdentifier() const;

  // Returns the unique persistent storage identifier for the service.
  virtual std::string GetStorageIdentifier() const = 0;

  // Returns whether the service configuration can be loaded from |storage|.
  virtual bool IsLoadableFrom(StoreInterface *storage) const;

  // Loads the service from persistent |storage|. Returns true on success.
  virtual bool Load(StoreInterface *storage);

  // Indicate to service that it is no longer persisted to storage.  It
  // should purge any stored profile state (e.g., credentials).  Returns
  // true to indicate that this service should also be unregistered from
  // the manager, false otherwise.
  virtual bool Unload();

  // Saves the service to persistent |storage|. Returns true on success.
  virtual bool Save(StoreInterface *storage);

  // Saves the service to the current profile.
  virtual void SaveToCurrentProfile();

  // Applies all the properties in |args| to this service object's mutable
  // store, except for those in parameters_ignored_for_configure_.
  // Returns an error in |error| if one or more parameter set attempts
  // fails, but will only return the first error.
  virtual void Configure(const KeyValueStore &args, Error *error);

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

  virtual void MakeFavorite();

  // Set the connection for this service.  If the connection is non-NULL, create
  // an HTTP Proxy that will utilize this service's connection to serve
  // requests.
  virtual void SetConnection(const ConnectionRefPtr &connection);
  virtual const ConnectionRefPtr &connection() const { return connection_; }

  // Examines the EAP credentials for the service and returns true if a
  // connection attempt can be made.
  bool Is8021xConnectable() const;

  // The inherited class should register any custom metrics in this method.
  virtual void InitializeCustomMetrics() const {}

  // The inherited class that needs to send metrics after the service has
  // transitioned to the ready state should override this method.
  // |time_resume_to_ready_milliseconds| holds the elapsed time from when
  // the system was resumed until when the service transitioned to the
  // connected state.  This value is non-zero for the first service transition
  // to the connected state after a resume.
  virtual void SendPostReadyStateMetrics(
      int64 /*time_resume_to_ready_milliseconds*/) const {}

  bool auto_connect() const { return auto_connect_; }
  void set_auto_connect(bool connect) { auto_connect_ = connect; }

  bool connectable() const { return connectable_; }
  void set_connectable(bool connectable);

  virtual bool explicitly_disconnected() const {
    return explicitly_disconnected_;
  }

  bool favorite() const { return favorite_; }
  // Setter is deliberately omitted; use MakeFavorite.

  const std::string &friendly_name() const { return friendly_name_; }
  void set_friendly_name(const std::string &n) { friendly_name_ = n; }

  const std::string &guid() const { return guid_; }
  void set_guid(const std::string &guid) { guid_ = guid; }

  int32 priority() const { return priority_; }
  void set_priority(int32 priority) { priority_ = priority; }

  int32 security_level() const { return security_level_; }
  void set_security_level(int32 security) { security_level_ = security; }

  void SetStrength(uint8 strength);

  // uint8 streams out as a char. Coerce to a larger type, so that
  // it prints as a number.
  uint16 strength() const { return strength_; }

  virtual Technology::Identifier technology() const { return technology_; }
  std::string GetTechnologyString(Error *error);

  const EapCredentials &eap() const { return eap_; }
  virtual void set_eap(const EapCredentials &eap);

  bool save_credentials() const { return save_credentials_; }
  void set_save_credentials(bool save) { save_credentials_ = save; }

  const std::string &error() const { return error_; }
  void set_error(const std::string &error) { error_ = error; }

  static const char *ConnectFailureToString(const ConnectFailure &state);
  static const char *ConnectStateToString(const ConnectState &state);

  // Compare two services.  Returns true if Service a should be displayed
  // above Service b
  static bool Compare(ServiceRefPtr a,
                      ServiceRefPtr b,
                      const std::vector<Technology::Identifier> &tech_order,
                      const char **reason);

  // These are defined in service.cc so that we don't have to include profile.h
  // TODO(cmasone): right now, these are here only so that we can get the
  // profile name as a property.  Can we store just the name, and then handle
  // setting the profile for this service via |manager_|?
  const ProfileRefPtr &profile() const;
  void set_profile(const ProfileRefPtr &p);

  // Notification that occurs when a service now has profile data saved
  // on its behalf.  Some service types like WiFi can choose to register
  // themselves at this point.
  virtual void OnProfileConfigured() {}

  // Notification that occurs when a single property has been changed via
  // the RPC adaptor.
  void OnPropertyChanged(const std::string &property);

  PropertyStore *mutable_store() { return &store_; }
  const PropertyStore &store() const { return store_; }
  const StaticIPParameters &static_ip_parameters() const {
    return static_ip_parameters_;
  }

 protected:
  friend class base::RefCounted<Service>;

  virtual ~Service();

  // Returns true if a character is allowed to be in a service storage id.
  static bool LegalChar(char a) { return isalnum(a) || a == '_'; }

  // Returns true if a character is disallowed to be in a service storage id.
  static bool IllegalChar(char a) { return !LegalChar(a); }

  virtual std::string CalculateState(Error *error);

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
      void(Service::*set)(const bool &value, Error *error));
  void HelpRegisterDerivedString(
      const std::string &name,
      std::string(Service::*get)(Error *error),
      void(Service::*set)(const std::string &value, Error *error));
  void HelpRegisterDerivedUint16(
      const std::string &name,
      uint16(Service::*get)(Error *error),
      void(Service::*set)(const uint16 &value, Error *error));
  void HelpRegisterDerivedRpcIdentifier(
      const std::string &name,
      std::string(Service::*get)(Error *),
      void(Service::*set)(const RpcIdentifier&, Error *));
  // Expose a property over RPC, with the name |name|.
  //
  // Reads of the property will be handled by invoking |get|.
  // Writes to the property will be handled by invoking |set|.
  //
  // Clearing the property will be handled by invoking |clear|, or
  // calling |set| with |default_value| (whichever is non-NULL).  It
  // is an error to call this method with both |clear| and
  // |default_value| non-NULL.
  void HelpRegisterWriteOnlyDerivedString(
      const std::string &name,
      void(Service::*set)(const std::string &value, Error *error),
      void(Service::*clear)(Error *error),
      const std::string *default_value);

  ServiceAdaptorInterface *adaptor() const { return adaptor_.get(); }

  // Assigns |value| to |key| in |storage| if |value| is non-empty and |save| is
  // true. Otherwise, removes |key| from |storage|. If |crypted| is true, the
  // value is encrypted.
  void SaveString(StoreInterface *storage,
                  const std::string &id,
                  const std::string &key,
                  const std::string &value,
                  bool crypted,
                  bool save);

  void LoadEapCredentials(StoreInterface *storage, const std::string &id);
  void SaveEapCredentials(StoreInterface *storage, const std::string &id);
  void UnloadEapCredentials();

  // Ignore |parameter| when performing a Configure() operation.
  void IgnoreParameterForConfigure(const std::string &parameter);

  // Property accessors reserved for subclasses
  EventDispatcher *dispatcher() const { return dispatcher_; }
  const std::string &GetEAPKeyManagement() const;
  void SetEAPKeyManagement(const std::string &key_management);
  void SetEAPPassword(const std::string &password, Error *error);
  void SetEAPPrivateKeyPassword(const std::string &password, Error *error);
  Manager *manager() const { return manager_; }
  Metrics *metrics() const { return metrics_; }

  void set_favorite(bool favorite) { favorite_ = favorite; }

 private:
  friend class MetricsTest;
  friend class ServiceAdaptorInterface;
  friend class VPNServiceTest;
  friend class WiFiServiceTest;
  FRIEND_TEST(ServiceTest, ConfigureIgnoredProperty);
  FRIEND_TEST(ServiceTest, ConfigureStringProperty);
  FRIEND_TEST(ServiceTest, Constructor);
  FRIEND_TEST(ServiceTest, GetIPConfigRpcIdentifier);
  FRIEND_TEST(ServiceTest, GetProperties);
  FRIEND_TEST(ServiceTest, IsAutoConnectable);
  FRIEND_TEST(ServiceTest, RecheckPortal);
  FRIEND_TEST(ServiceTest, Save);
  FRIEND_TEST(ServiceTest, SaveString);
  FRIEND_TEST(ServiceTest, SaveStringCrypted);
  FRIEND_TEST(ServiceTest, SaveStringDontSave);
  FRIEND_TEST(ServiceTest, SaveStringEmpty);
  FRIEND_TEST(ServiceTest, SetProperty);
  FRIEND_TEST(ServiceTest, SetCheckPortal);
  FRIEND_TEST(ServiceTest, State);
  FRIEND_TEST(ServiceTest, Unload);

  static const char kAutoConnConnected[];
  static const char kAutoConnConnecting[];
  static const char kAutoConnExplicitDisconnect[];
  static const char kAutoConnNotConnectable[];

  static const char kServiceSortAutoConnect[];
  static const char kServiceSortConnectable[];
  static const char kServiceSortFavorite[];
  static const char kServiceSortIsConnected[];
  static const char kServiceSortIsConnecting[];
  static const char kServiceSortIsFailed[];
  static const char kServiceSortPriority[];
  static const char kServiceSortSecurityEtc[];
  static const char kServiceSortTechnology[];
  static const char kServiceSortUniqueName[];

  bool GetAutoConnect(Error *error);
  void SetAutoConnect(const bool &connect, Error *error);

  std::string GetCheckPortal(Error *error);
  void SetCheckPortal(const std::string &check_portal, Error *error);

  virtual std::string GetDeviceRpcId(Error *error) = 0;

  std::string GetIPConfigRpcIdentifier(Error *error);

  std::string GetNameProperty(Error *error);
  void AssertTrivialSetNameProperty(const std::string &name, Error *error);

  std::string GetProfileRpcId(Error *error);
  void SetProfileRpcId(const std::string &profile, Error *error);

  // Returns TCP port of service's HTTP proxy in host order.
  uint16 GetHTTPProxyPort(Error *error);

  // Utility function that returns true if a is different from b.  When they
  // are, "decision" is populated with the boolean value of "a > b".
  static bool DecideBetween(int a, int b, bool *decision);

  ConnectState state_;
  ConnectFailure failure_;
  bool auto_connect_;
  std::string check_portal_;
  bool connectable_;
  std::string error_;
  bool explicitly_disconnected_;
  bool favorite_;
  int32 priority_;
  int32 security_level_;
  uint8 strength_;
  std::string proxy_config_;
  std::string ui_data_;
  std::string guid_;
  bool save_credentials_;
  EapCredentials eap_;  // Only saved if |save_credentials_| is true.
  Technology::Identifier technology_;
  // The time of the most recent failure. Value is 0 if the service is
  // not currently failed.
  time_t failed_time_;

  ProfileRefPtr profile_;
  PropertyStore store_;
  std::set<std::string> parameters_ignored_for_configure_;

  EventDispatcher *dispatcher_;
  static unsigned int serial_number_;
  std::string unique_name_;  // MUST be unique amongst service instances
  std::string friendly_name_;  // MAY be same as |unique_name_|
  bool available_;
  bool configured_;
  Configuration *configuration_;
  scoped_ptr<ServiceAdaptorInterface> adaptor_;
  scoped_ptr<HTTPProxy> http_proxy_;
  ConnectionRefPtr connection_;
  StaticIPParameters static_ip_parameters_;
  Metrics *metrics_;
  Manager *manager_;
  scoped_ptr<Sockets> sockets_;

  DISALLOW_COPY_AND_ASSIGN(Service);
};

}  // namespace shill

#endif  // SHILL_SERVICE_
