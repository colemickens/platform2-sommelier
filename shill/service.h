// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
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

namespace shill {

class Connection;
class Configuration;
class ControlInterface;
class Endpoint;
class Error;
class EventDispatcher;
class Manager;
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
    kServiceFailureUnknown,
    kServiceFailureActivationFailure,
    kServiceFailureOutOfRange,
    kServiceFailurePinMissing,
    kServiceFailureConfigurationFailed,
    kServiceFailureBadCredentials,
    kServiceFailureNeedEVDO,
    kServiceFailureNeedHomeNetwork,
    kServiceFailureOTASPFailure,
    kServiceFailureAAAFailure
  };
  enum ConnectState {
    kServiceStateUnknown,
    kServiceStateIdle,
    kServiceStateAssociating,
    kServiceStateConfiguring,
    kServiceStateConnected,
    kServiceStateDisconnected,
    kServiceStateFailure
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

  // A constructor for the Service object
  Service(ControlInterface *control_interface,
          EventDispatcher *dispatcher,
          Manager *manager);
  virtual ~Service();

  virtual void Connect() = 0;
  virtual void Disconnect() = 0;

  virtual bool IsActive() { return false; }

  // Returns a string that is guaranteed to uniquely identify this Service
  // instance.
  const std::string &UniqueName() const { return name_; }

  virtual std::string GetRpcIdentifier() const;

  // Returns the unique persistent storage identifier for the service.
  std::string GetStorageIdentifier();

  // Loads the service from persistent |storage|. Returns true on success.
  virtual bool Load(StoreInterface *storage);

  // Saves the service to persistent |storage|. Returns true on success.
  virtual bool Save(StoreInterface *storage);

  bool auto_connect() const { return auto_connect_; }
  void set_auto_connect(bool connect) { auto_connect_ = connect; }

  const ProfileRefPtr &profile() const;
  void set_profile(const ProfileRefPtr &p);

  PropertyStore *store() { return &store_; }

 protected:
  static const int kPriorityNone = 0;

  virtual std::string CalculateState() = 0;

  void HelpRegisterDerivedBool(const std::string &name,
                               bool(Service::*get)(void),
                               bool(Service::*set)(const bool&));
  void HelpRegisterDerivedString(const std::string &name,
                                 std::string(Service::*get)(void),
                                 bool(Service::*set)(const std::string&));

  // Assigns |value| to |key| in |storage| if |value| is non-empty and |save| is
  // true. Otherwise, removes |key| from |storage|. If |crypted| is true, the
  // value is encrypted.
  void SaveString(StoreInterface *storage,
                  const std::string &key,
                  const std::string &value,
                  bool crypted,
                  bool save);

  void LoadEapCredentials(StoreInterface *storage);
  void SaveEapCredentials(StoreInterface *storage);

  // Properties
  bool auto_connect_;
  std::string check_portal_;
  bool connectable_;
  std::string error_;
  bool favorite_;
  int32 priority_;
  std::string proxy_config_;
  bool save_credentials_;
  EapCredentials eap_;  // Only saved if |save_credentials_| is true.

  ProfileRefPtr profile_;
  PropertyStore store_;

  EventDispatcher *dispatcher_;

 private:
  friend class ServiceAdaptorInterface;
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

  virtual std::string GetDeviceRpcId() = 0;

  std::string GetProfileRpcId() {
    return "";  // Will need to call Profile to get this.
  }

  static unsigned int serial_number_;
  const std::string name_;
  bool available_;
  bool configured_;
  Configuration *configuration_;
  Connection *connection_;
  scoped_ptr<ServiceAdaptorInterface> adaptor_;
  Manager *manager_;

  DISALLOW_COPY_AND_ASSIGN(Service);
};

}  // namespace shill

#endif  // SHILL_SERVICE_
