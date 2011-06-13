// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <time.h>
#include <stdio.h>

#include <map>
#include <string>
#include <vector>

#include <base/logging.h>
#include <base/memory/scoped_ptr.h>
#include <chromeos/dbus/service_constants.h>

#include "shill/control_interface.h"
#include "shill/device_config_interface.h"
#include "shill/error.h"
#include "shill/property_accessor.h"
#include "shill/service.h"
#include "shill/service_dbus_adaptor.h"

using std::map;
using std::string;
using std::vector;

namespace shill {
Service::Service(ControlInterface *control_interface,
                 EventDispatcher *dispatcher,
                 DeviceConfigInterfaceRefPtr config_interface,
                 const string& name)
    : auto_connect_(false),
      connectable_(false),
      favorite_(false),
      priority_(0),
      save_credentials_(true),
      dispatcher_(dispatcher),
      name_(name),
      available_(false),
      configured_(false),
      configuration_(NULL),
      connection_(NULL),
      config_interface_(config_interface),
      adaptor_(control_interface->CreateServiceAdaptor(this)) {

  RegisterBool(flimflam::kAutoConnectProperty, &auto_connect_);
  RegisterString(flimflam::kCheckPortalProperty, &check_portal_);
  RegisterConstBool(flimflam::kConnectableProperty, &connectable_);
  RegisterDerivedString(flimflam::kDeviceProperty, &Service::DeviceRPCID, NULL);

  RegisterString(flimflam::kEapIdentityProperty, &eap_.identity);
  RegisterString(flimflam::kEAPEAPProperty, &eap_.eap);
  RegisterString(flimflam::kEapPhase2AuthProperty, &eap_.inner_eap);
  RegisterString(flimflam::kEapAnonymousIdentityProperty,
                 &eap_.anonymous_identity);
  RegisterString(flimflam::kEAPClientCertProperty, &eap_.client_cert);
  RegisterString(flimflam::kEAPCertIDProperty, &eap_.cert_id);
  RegisterString(flimflam::kEapPrivateKeyProperty, &eap_.private_key);
  RegisterString(flimflam::kEapPrivateKeyPasswordProperty,
                 &eap_.private_key_password);
  RegisterString(flimflam::kEAPKeyIDProperty, &eap_.key_id);
  RegisterString(flimflam::kEapCaCertProperty, &eap_.ca_cert);
  RegisterString(flimflam::kEapCaCertIDProperty, &eap_.ca_cert_id);
  RegisterString(flimflam::kEAPPINProperty, &eap_.pin);
  RegisterString(flimflam::kEapPasswordProperty, &eap_.password);
  RegisterString(flimflam::kEapKeyMgmtProperty, &eap_.key_management);
  RegisterBool(flimflam::kEapUseSystemCAsProperty, &eap_.use_system_cas);

  RegisterConstString(flimflam::kErrorProperty, &error_);
  RegisterConstBool(flimflam::kFavoriteProperty, &favorite_);
  RegisterDerivedBool(flimflam::kIsActiveProperty, &Service::IsActive, NULL);
  RegisterConstString(flimflam::kNameProperty, &name_);
  RegisterInt32(flimflam::kPriorityProperty, &priority_);
  RegisterDerivedString(flimflam::kProfileProperty,
                        &Service::ProfileRPCID,
                        NULL);
  RegisterString(flimflam::kProxyConfigProperty, &proxy_config_);
  RegisterBool(flimflam::kSaveCredentialsProperty, &save_credentials_);
  RegisterDerivedString(flimflam::kStateProperty,
                        &Service::CalculateState,
                        NULL);

  // TODO(cmasone): Create VPN Service with this property
  // RegisterConstStringmap(flimflam::kProviderProperty, &provider_);

  // TODO(cmasone): Add support for R/O properties that return DBus object paths
  // flimflam::kDeviceProperty, flimflam::kProfileProperty

  VLOG(2) << "Service initialized.";
}

Service::~Service() {}

bool Service::Contains(const string &property) {
  return (bool_properties_.find(property) != bool_properties_.end() ||
          int32_properties_.find(property) != int32_properties_.end() ||
          string_properties_.find(property) != string_properties_.end());
}

bool Service::SetBoolProperty(const string& name, bool value, Error *error) {
  VLOG(2) << "Setting " << name << " as a bool.";
  bool set = bool_properties_[name]->Set(value);
  if (!set && error)
    error->Populate(Error::kInvalidArguments, name + " is not a R/W bool.");
  return set;
}

bool Service::SetInt32Property(const string& name, int32 value, Error *error) {
  VLOG(2) << "Setting " << name << " as an int32";
  bool set = int32_properties_[name]->Set(value);
  if (!set && error)
    error->Populate(Error::kInvalidArguments, name + " is not a R/W int32.");
  return set;
}

bool Service::SetStringProperty(const string& name,
                                const string& value,
                                Error *error) {
  VLOG(2) << "Setting " << name << " as a string.";
  bool set = string_properties_[name]->Set(value);
  if (!set && error)
    error->Populate(Error::kInvalidArguments, name + " is not a R/W string.");
  return set;
}

bool Service::SetStringmapProperty(const string& name,
                                   const map<string, string>& value,
                                   Error *error) {
  VLOG(2) << "Setting " << name << " as a string map.";
  bool set = stringmap_properties_[name]->Set(value);
  if (!set && error)
    error->Populate(Error::kInvalidArguments, name + " is not a R/W strmap.");
  return set;
}

void Service::RegisterBool(const string &name, bool *prop) {
  bool_properties_[name] = BoolAccessor(new PropertyAccessor<bool>(prop));
}

void Service::RegisterConstBool(const string &name, const bool *prop) {
  bool_properties_[name] = BoolAccessor(new ConstPropertyAccessor<bool>(prop));
}

void Service::RegisterDerivedBool(const string &name,
                                  bool(Service::*get)(void),
                                  bool(Service::*set)(const bool&)) {
  bool_properties_[name] =
      BoolAccessor(new CustomAccessor<Service, bool>(this, get, set));
}

void Service::RegisterInt32(const string &name, int32 *prop) {
  int32_properties_[name] = Int32Accessor(new PropertyAccessor<int32>(prop));
}

void Service::RegisterString(const string &name, string *prop) {
  string_properties_[name] = StringAccessor(new PropertyAccessor<string>(prop));
}

void Service::RegisterConstString(const string &name, const string *prop) {
  string_properties_[name] =
      StringAccessor(new ConstPropertyAccessor<string>(prop));
}

void Service::RegisterDerivedString(const string &name,
                                    string(Service::*get)(void),
                                    bool(Service::*set)(const string&)) {
  string_properties_[name] =
      StringAccessor(new CustomAccessor<Service, string>(this, get, set));
}

void Service::RegisterStringmap(const string &name,
                                map<string, string> *prop) {
  stringmap_properties_[name] =
      StringmapAccessor(new PropertyAccessor<map<string, string> >(prop));
}

void Service::RegisterConstStringmap(const string &name,
                                     const map<string, string> *prop) {
  stringmap_properties_[name] =
      StringmapAccessor(new ConstPropertyAccessor<map<string, string> >(prop));
}

void Service::RegisterConstUint8(const string &name, const uint8 *prop) {
  uint8_properties_[name] =
      Uint8Accessor(new ConstPropertyAccessor<uint8>(prop));
}

void Service::RegisterConstUint16(const string &name, const uint16 *prop) {
  uint16_properties_[name] =
      Uint16Accessor(new ConstPropertyAccessor<uint16>(prop));
}

}  // namespace shill
