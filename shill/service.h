// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_SERVICE_
#define SHILL_SERVICE_

#include <string>
#include <map>
#include <vector>

#include <base/memory/ref_counted.h>
#include <base/memory/scoped_ptr.h>
#include <gtest/gtest_prod.h>  // for FRIEND_TEST

#include "shill/accessor_interface.h"
#include "shill/property_store.h"
#include "shill/refptr_types.h"
#include "shill/sockets.h"
#include "shill/technology.h"

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
    kStateReady,
    kStatePortal,
    kStateFailure,
    kStateOnline
  };
  struct EapCredentials {
    EapCredentials() : use_system_cas(false) {}
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
    bool use_system_cas;
    std::string pin;
    std::string password;
    std::string key_management;
  };

  static const int kPriorityNone;

  // A constructor for the Service object
  Service(ControlInterface *control_interface,
          EventDispatcher *dispatcher,
          Manager *manager,
          Technology::Identifier technology);
  virtual ~Service();

  // AutoConnect MAY choose to ignore the connection request in some
  // cases. For example, if the corresponding Device only supports one
  // concurrent connection, and another Service is already connected
  // or connecting.
  //
  // AutoConnect MAY issue RPCs immediately. So AutoConnect MUST NOT
  // be called from a D-Bus signal handler context.
  virtual void AutoConnect();
  // Queue up a connection attempt.
  virtual void Connect(Error *error) = 0;
  virtual void Disconnect(Error *error) = 0;

  // The default implementation sets |error| to kInvalidArguments.
  virtual void ActivateCellularModem(const std::string &carrier, Error *error);

  // Base method always returns false.
  virtual bool TechnologyIs(const Technology::Identifier type) const;

  virtual bool IsActive(Error *error);

  virtual ConnectState state() const { return state_; }
  // Updates the state of the Service and alerts the manager.  Also
  // clears |failure_| if the new state isn't a failure.
  virtual void SetState(ConnectState state);

  // State utility functions
  virtual bool IsConnected() const { return state() == kStateConnected; }
  virtual bool IsConnecting() const {
    return state() == kStateAssociating || state() == kStateConfiguring;
  }
  virtual bool IsFailed() const {
    return state() == kStateFailure;
  }

  virtual ConnectFailure failure() const { return failure_; }
  // Records the failure mode, and sets the Service state to "Failure".
  virtual void SetFailure(ConnectFailure failure);

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
  // should purge any stored profile state (e.g., credentials).
  virtual void Unload();

  // Saves the service to persistent |storage|. Returns true on success.
  virtual bool Save(StoreInterface *storage);

  // Returns true if the service RPC identifier should be part of the
  // manager's advertised services list, false otherwise.
  virtual bool IsVisible() const { return true; }

  virtual void MakeFavorite();

  // Set the connection for this service.  If the connection is
  // non-NULL, create an HTTP Proxy that will utilize this service's
  // connection to serve requests.
  virtual void SetConnection(ConnectionRefPtr connection);

  // The inherited class should register any custom metrics in this method.
  virtual void InitializeCustomMetrics() const {}

  // The inherited class that needs to send metrics after the service has
  // transitioned to the ready state.
  virtual void SendPostReadyStateMetrics() const {}

  bool auto_connect() const { return auto_connect_; }
  void set_auto_connect(bool connect) { auto_connect_ = connect; }

  bool connectable() const { return connectable_; }
  void set_connectable(bool connectable);

  bool favorite() const { return favorite_; }
  // Setter is deliberately omitted; use MakeFavorite.

  const std::string &friendly_name() const { return friendly_name_; }

  int32 priority() const { return priority_; }
  void set_priority(int32 priority) { priority_ = priority; }

  int32 security_level() const { return security_level_; }
  void set_security_level(int32 security) { security_level_ = security; }

  int32 strength() const { return strength_; }
  void set_strength(int32 strength) { strength_ = strength; }

  virtual Technology::Identifier technology() const { return technology_; }
  std::string GetTechnologyString(Error *error);

  const std::string &error() const { return error_; }
  void set_error(const std::string &error) { error_ = error; }

  static const char *ConnectFailureToString(const ConnectFailure &state);
  static const char *ConnectStateToString(const ConnectState &state);

  // Compare two services.  Returns true if Service a should be displayed
  // above Service b
  static bool Compare(ServiceRefPtr a,
                      ServiceRefPtr b,
                      const std::vector<Technology::Identifier> &tech_order);

  // These are defined in service.cc so that we don't have to include profile.h
  // TODO(cmasone): right now, these are here only so that we can get the
  // profile name as a property.  Can we store just the name, and then handle
  // setting the profile for this service via |manager_|?
  const ProfileRefPtr &profile() const;
  void set_profile(const ProfileRefPtr &p);

  PropertyStore *mutable_store() { return &store_; }
  const PropertyStore &store() const { return store_; }
  const ConnectionRefPtr &connection() const { return connection_; }

 protected:
  // Returns true if a character is allowed to be in a service storage id.
  static bool LegalChar(char a) { return isalnum(a) || a == '_'; }

  void set_friendly_name(const std::string &n) { friendly_name_ = n; }

  virtual std::string CalculateState(Error *error);

  // Returns whether this service is in a state conducive to auto-connect.
  // This should include any tests used for computing connectable(),
  // as well as other critera such as whether the device associated with
  // this service is busy with another connection.
  virtual bool IsAutoConnectable() const { return connectable(); }

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

  // Property accessors reserved for subclasses
  EventDispatcher *dispatcher() const { return dispatcher_; }
  const std::string &GetEAPKeyManagement() const;
  void SetEAPKeyManagement(const std::string &key_management);
  Metrics *metrics() const { return metrics_; }

 private:
  friend class ServiceAdaptorInterface;
  friend class MetricsTest;
  FRIEND_TEST(DeviceTest, SelectedService);
  FRIEND_TEST(ManagerTest, SortServicesWithConnection);
  FRIEND_TEST(ServiceTest, Constructor);
  FRIEND_TEST(ServiceTest, Save);
  FRIEND_TEST(ServiceTest, SaveString);
  FRIEND_TEST(ServiceTest, SaveStringCrypted);
  FRIEND_TEST(ServiceTest, SaveStringDontSave);
  FRIEND_TEST(ServiceTest, SaveStringEmpty);

  static const char kStorageAutoConnect[];
  static const char kStorageCheckPortal[];
  static const char kStorageEapAnonymousIdentity[];
  static const char kStorageEapCACert[];
  static const char kStorageEapCACertID[];
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
  static const char kStorageFavorite[];
  static const char kStorageName[];
  static const char kStoragePriority[];
  static const char kStorageProxyConfig[];
  static const char kStorageSaveCredentials[];
  static const char kStorageType[];
  static const char kStorageUIData[];

  virtual std::string GetDeviceRpcId(Error *error) = 0;

  std::string GetProfileRpcId(Error */*error*/) {
    return "";  // Will need to call Profile to get this.
  }

  // Returns TCP port of service's HTTP proxy in host order.
  uint16 GetHTTPProxyPort(Error *error);

  // Utility function that returns true if a is different from b.  When they
  // are, "decision" is populated with the boolean value of "a > b".
  static bool DecideBetween(int a, int b, bool *decision);

  // For unit testing.
  void set_metrics(Metrics *metrics) { metrics_ = metrics; }

  ConnectState state_;
  ConnectFailure failure_;
  bool auto_connect_;
  std::string check_portal_;
  bool connectable_;
  std::string error_;
  bool favorite_;
  int32 priority_;
  int32 security_level_;
  int32 strength_;
  std::string proxy_config_;
  std::string ui_data_;
  bool save_credentials_;
  EapCredentials eap_;  // Only saved if |save_credentials_| is true.
  Technology::Identifier technology_;

  ProfileRefPtr profile_;
  PropertyStore store_;

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
  Manager *manager_;
  Sockets sockets_;
  Metrics *metrics_;

  DISALLOW_COPY_AND_ASSIGN(Service);
};

}  // namespace shill

#endif  // SHILL_SERVICE_
