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
#include "shill/entry.h"
#include "shill/error.h"
#include "shill/profile.h"
#include "shill/property_accessor.h"
#include "shill/refptr_types.h"
#include "shill/service.h"
#include "shill/service_dbus_adaptor.h"

using std::map;
using std::string;
using std::vector;

namespace shill {
Service::Service(ControlInterface *control_interface,
                 EventDispatcher *dispatcher,
                 const ProfileRefPtr &profile,
                 const EntryRefPtr &entry,
                 const string& name)
    : auto_connect_(false),
      connectable_(false),
      favorite_(false),
      priority_(0),
      profile_(profile),
      entry_(entry),
      dispatcher_(dispatcher),
      name_(name),
      available_(false),
      configured_(false),
      configuration_(NULL),
      connection_(NULL),
      adaptor_(control_interface->CreateServiceAdaptor(this)) {

  store_.RegisterBool(flimflam::kAutoConnectProperty, &auto_connect_);

  // flimflam::kActivationStateProperty: Registered in CellularService
  // flimflam::kCellularApnProperty: Registered in CellularService
  // flimflam::kCellularLastGoodApnProperty: Registered in CellularService
  // flimflam::kNetworkTechnologyProperty: Registered in CellularService
  // flimflam::kOperatorNameProperty: Registered in CellularService
  // flimflam::kOperatorCodeProperty: Registered in CellularService
  // flimflam::kRoamingStateProperty: Registered in CellularService
  // flimflam::kPaymentURLProperty: Registered in CellularService

  store_.RegisterString(flimflam::kCheckPortalProperty, &check_portal_);
  store_.RegisterConstBool(flimflam::kConnectableProperty, &connectable_);
  HelpRegisterDerivedString(flimflam::kDeviceProperty,
                            &Service::GetDeviceRpcId,
                            NULL);

  store_.RegisterString(flimflam::kEapIdentityProperty, &entry_->eap.identity);
  store_.RegisterString(flimflam::kEAPEAPProperty, &entry_->eap.eap);
  store_.RegisterString(flimflam::kEapPhase2AuthProperty,
                        &entry_->eap.inner_eap);
  store_.RegisterString(flimflam::kEapAnonymousIdentityProperty,
                        &entry_->eap.anonymous_identity);
  store_.RegisterString(flimflam::kEAPClientCertProperty,
                        &entry_->eap.client_cert);
  store_.RegisterString(flimflam::kEAPCertIDProperty, &entry_->eap.cert_id);
  store_.RegisterString(flimflam::kEapPrivateKeyProperty,
                        &entry_->eap.private_key);
  store_.RegisterString(flimflam::kEapPrivateKeyPasswordProperty,
                        &entry_->eap.private_key_password);
  store_.RegisterString(flimflam::kEAPKeyIDProperty, &entry_->eap.key_id);
  store_.RegisterString(flimflam::kEapCaCertProperty, &entry_->eap.ca_cert);
  store_.RegisterString(flimflam::kEapCaCertIDProperty,
                        &entry_->eap.ca_cert_id);
  store_.RegisterString(flimflam::kEAPPINProperty, &entry_->eap.pin);
  store_.RegisterString(flimflam::kEapPasswordProperty, &entry_->eap.password);
  store_.RegisterString(flimflam::kEapKeyMgmtProperty,
                        &entry_->eap.key_management);
  store_.RegisterBool(flimflam::kEapUseSystemCAsProperty,
                      &entry_->eap.use_system_cas);

  store_.RegisterConstString(flimflam::kErrorProperty, &error_);
  store_.RegisterConstBool(flimflam::kFavoriteProperty, &favorite_);
  HelpRegisterDerivedBool(flimflam::kIsActiveProperty,
                          &Service::IsActive, NULL);
  // flimflam::kModeProperty: Registered in WiFiService
  store_.RegisterConstString(flimflam::kNameProperty, &name_);
  // flimflam::kPassphraseProperty: Registered in WiFiService
  // flimflam::kPassphraseRequiredProperty: Registered in WiFiService
  store_.RegisterInt32(flimflam::kPriorityProperty, &priority_);
  HelpRegisterDerivedString(flimflam::kProfileProperty,
                            &Service::GetProfileRpcId,
                            NULL);
  store_.RegisterString(flimflam::kProxyConfigProperty, &proxy_config_);
  // TODO(cmasone): Create VPN Service with this property
  // store_.RegisterConstStringmap(flimflam::kProviderProperty, &provider_);

  store_.RegisterBool(flimflam::kSaveCredentialsProperty,
                      &entry_->save_credentials);
  // flimflam::kSecurityProperty: Registered in WiFiService
  HelpRegisterDerivedString(flimflam::kStateProperty,
                            &Service::CalculateState,
                            NULL);
  // flimflam::kSignalStrengthProperty: Registered in WiFi/CellularService
  // flimflam::kTypeProperty: Registered in all derived classes.
  // flimflam::kWifiAuthMode: Registered in WiFiService
  // flimflam::kWifiHiddenSsid: Registered in WiFiService
  // flimflam::kWifiFrequency: Registered in WiFiService
  // flimflam::kWifiPhyMode: Registered in WiFiService
  // flimflam::kWifiHexSsid: Registered in WiFiService
  VLOG(2) << "Service initialized.";
}

Service::~Service() {}

string Service::GetRpcIdentifier() {
  return adaptor_->GetRpcIdentifier();
}

void Service::HelpRegisterDerivedBool(const string &name,
                                  bool(Service::*get)(void),
                                  bool(Service::*set)(const bool&)) {
  store_.RegisterDerivedBool(
      name,
      BoolAccessor(new CustomAccessor<Service, bool>(this, get, set)));
}

void Service::HelpRegisterDerivedString(const string &name,
                                    string(Service::*get)(void),
                                    bool(Service::*set)(const string&)) {
  store_.RegisterDerivedString(
      name,
      StringAccessor(new CustomAccessor<Service, string>(this, get, set)));
}

}  // namespace shill
