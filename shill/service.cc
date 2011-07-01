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
#include <base/stl_util-inl.h>
#include <chromeos/dbus/service_constants.h>

#include "shill/control_interface.h"
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
      adaptor_(control_interface->CreateServiceAdaptor(this)) {

  RegisterBool(flimflam::kAutoConnectProperty, &auto_connect_);
  RegisterString(flimflam::kCheckPortalProperty, &check_portal_);
  RegisterConstBool(flimflam::kConnectableProperty, &connectable_);
  RegisterDerivedString(flimflam::kDeviceProperty,
                        &Service::GetDeviceRpcId,
                        NULL);

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
                        &Service::GetProfileRpcId,
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

bool Service::SetBoolProperty(const string& name, bool value, Error *error) {
  VLOG(2) << "Setting " << name << " as a bool.";
  bool set = (ContainsKey(bool_properties_, name) &&
              bool_properties_[name]->Set(value));
  if (!set && error)
    error->Populate(Error::kInvalidArguments, name + " is not a R/W bool.");
  return set;
}

bool Service::SetInt32Property(const string& name, int32 value, Error *error) {
  VLOG(2) << "Setting " << name << " as an int32";
  bool set = (ContainsKey(int32_properties_, name) &&
              int32_properties_[name]->Set(value));
  if (!set && error)
    error->Populate(Error::kInvalidArguments, name + " is not a R/W int32.");
  return set;
}

bool Service::SetStringProperty(const string& name,
                                const string& value,
                                Error *error) {
  VLOG(2) << "Setting " << name << " as a string.";
  bool set = (ContainsKey(string_properties_, name) &&
              string_properties_[name]->Set(value));
  if (!set && error)
    error->Populate(Error::kInvalidArguments, name + " is not a R/W string.");
  return set;
}

bool Service::SetStringmapProperty(const string& name,
                                   const map<string, string>& value,
                                   Error *error) {
  VLOG(2) << "Setting " << name << " as a string map.";
  bool set = (ContainsKey(stringmap_properties_, name) &&
              stringmap_properties_[name]->Set(value));
  if (!set && error)
    error->Populate(Error::kInvalidArguments, name + " is not a R/W strmap.");
  return set;
}

string Service::GetRpcIdentifier() {
  return adaptor_->GetRpcIdentifier();
}

void Service::RegisterDerivedBool(const string &name,
                                  bool(Service::*get)(void),
                                  bool(Service::*set)(const bool&)) {
  bool_properties_[name] =
      BoolAccessor(new CustomAccessor<Service, bool>(this, get, set));
}

void Service::RegisterDerivedString(const string &name,
                                    string(Service::*get)(void),
                                    bool(Service::*set)(const string&)) {
  string_properties_[name] =
      StringAccessor(new CustomAccessor<Service, string>(this, get, set));
}

}  // namespace shill
