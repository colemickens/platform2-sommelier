// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_SERVICE_
#define SHILL_SERVICE_

#include <string>
#include <map>
#include <vector>


#include <base/hash_tables.h>
#include <base/memory/ref_counted.h>
#include <base/memory/scoped_ptr.h>

#include "shill/device_config_interface.h"
#include "shill/accessor_interface.h"
#include "shill/property_store_interface.h"

namespace shill {

class Connection;
class Configuration;
class ControlInterface;
class Endpoint;
class Error;
class EventDispatcher;
class Service;
class ServiceAdaptorInterface;

typedef scoped_refptr<const Service> ServiceConstRefPtr;
typedef scoped_refptr<Service> ServiceRefPtr;

class Service : public base::RefCounted<Service>,
                public PropertyStoreInterface {
 public:
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
          DeviceConfigInterfaceRefPtr config_interface,
          const std::string& name);
  virtual ~Service();
  virtual void Connect() = 0;
  virtual void Disconnect() = 0;

  virtual bool IsActive() { return false; }

  // Implementation of PropertyStoreInterface
  virtual bool Contains(const std::string &property);
  virtual bool SetBoolProperty(const std::string &name,
                               bool value,
                               Error *error);
  virtual bool SetInt32Property(const std::string &name,
                                int32 value,
                                Error *error);
  virtual bool SetStringProperty(const std::string &name,
                                 const std::string &value,
                                 Error *error);
  virtual bool SetStringmapProperty(
      const std::string &name,
      const std::map<std::string, std::string> &values,
      Error *error);

  // Returns a string that is guaranteed to uniquely identify this
  // Service instance.
  virtual const std::string &UniqueName() { return name_; }

  bool auto_connect() const { return auto_connect_; }
  void set_auto_connect(bool connect) { auto_connect_ = connect; }

 protected:
  virtual std::string CalculateState() = 0;

  void RegisterBool(const std::string &name, bool *prop);
  void RegisterConstBool(const std::string &name, const bool *prop);
  void RegisterDerivedBool(const std::string &name,
                           bool(Service::*get)(void),
                           bool(Service::*set)(const bool&));
  void RegisterInt32(const std::string &name, int32 *prop);
  void RegisterString(const std::string &name, std::string *prop);
  void RegisterConstString(const std::string &name, const std::string *prop);
  void RegisterDerivedString(const std::string &name,
                             std::string(Service::*get)(void),
                             bool(Service::*set)(const std::string&));
  void RegisterStringmap(const std::string &name,
                         std::map<std::string, std::string> *prop);
  void RegisterConstStringmap(const std::string &name,
                              const std::map<std::string, std::string> *prop);
  void RegisterConstUint8(const std::string &name, const uint8 *prop);
  void RegisterConstUint16(const std::string &name, const uint16 *prop);

  base::hash_map<std::string, BoolAccessor> bool_properties_;
  base::hash_map<std::string, Int32Accessor> int32_properties_;
  base::hash_map<std::string, StringAccessor> string_properties_;
  base::hash_map<std::string, StringmapAccessor> stringmap_properties_;
  base::hash_map<std::string, Uint8Accessor> uint8_properties_;
  base::hash_map<std::string, Uint16Accessor> uint16_properties_;

  // Properties
  bool auto_connect_;
  std::string check_portal_;
  bool connectable_;
  EapCredentials eap_;
  std::string error_;
  bool favorite_;
  int32 priority_;
  std::string proxy_config_;
  bool save_credentials_;

  EventDispatcher *dispatcher_;

 private:
  std::string DeviceRPCID() {
    return "";  // Will need to call Device to get this.
  }
  std::string ProfileRPCID() {
    return "";  // Will need to call Profile to get this.
  }

  const std::string name_;
  bool available_;
  bool configured_;
  Configuration *configuration_;
  Connection *connection_;
  DeviceConfigInterfaceRefPtr config_interface_;
  Endpoint *endpoint_;
  scoped_ptr<ServiceAdaptorInterface> adaptor_;

  friend class ServiceAdaptorInterface;
  DISALLOW_COPY_AND_ASSIGN(Service);
};

}  // namespace shill

#endif  // SHILL_SERVICE_
