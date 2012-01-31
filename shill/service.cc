// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/service.h"

#include <time.h>
#include <stdio.h>

#include <map>
#include <string>
#include <vector>

#include <base/logging.h>
#include <base/memory/scoped_ptr.h>
#include <base/string_number_conversions.h>
#include <chromeos/dbus/service_constants.h>

#include "shill/connection.h"
#include "shill/control_interface.h"
#include "shill/error.h"
#include "shill/http_proxy.h"
#include "shill/manager.h"
#include "shill/metrics.h"
#include "shill/profile.h"
#include "shill/property_accessor.h"
#include "shill/refptr_types.h"
#include "shill/service_dbus_adaptor.h"
#include "shill/store_interface.h"

using std::map;
using std::string;
using std::vector;

namespace shill {

const char Service::kCheckPortalAuto[] = "auto";
const char Service::kCheckPortalFalse[] = "false";
const char Service::kCheckPortalTrue[] = "true";

const int Service::kPriorityNone = 0;

const char Service::kStorageAutoConnect[] = "AutoConnect";
const char Service::kStorageCheckPortal[] = "CheckPortal";
const char Service::kStorageEapAnonymousIdentity[] = "EAP.AnonymousIdentity";
const char Service::kStorageEapCACert[] = "EAP.CACert";
const char Service::kStorageEapCACertID[] = "EAP.CACertID";
const char Service::kStorageEapCertID[] = "EAP.CertID";
const char Service::kStorageEapClientCert[] = "EAP.ClientCert";
const char Service::kStorageEapEap[] = "EAP.EAP";
const char Service::kStorageEapIdentity[] = "EAP.Identity";
const char Service::kStorageEapInnerEap[] = "EAP.InnerEAP";
const char Service::kStorageEapKeyID[] = "EAP.KeyID";
const char Service::kStorageEapKeyManagement[] = "EAP.KeyMgmt";
const char Service::kStorageEapPIN[] = "EAP.PIN";
const char Service::kStorageEapPassword[] = "EAP.Password";
const char Service::kStorageEapPrivateKey[] = "EAP.PrivateKey";
const char Service::kStorageEapPrivateKeyPassword[] = "EAP.PrivateKeyPassword";
const char Service::kStorageEapUseSystemCAs[] = "EAP.UseSystemCAs";
const char Service::kStorageFavorite[] = "Favorite";
const char Service::kStorageName[] = "Name";
const char Service::kStoragePriority[] = "Priority";
const char Service::kStorageProxyConfig[] = "ProxyConfig";
const char Service::kStorageSaveCredentials[] = "SaveCredentials";
const char Service::kStorageType[] = "Type";
const char Service::kStorageUIData[] = "UIData";

// static
unsigned int Service::serial_number_ = 0;

Service::Service(ControlInterface *control_interface,
                 EventDispatcher *dispatcher,
                 Metrics *metrics,
                 Manager *manager,
                 Technology::Identifier technology)
    : state_(kStateUnknown),
      failure_(kFailureUnknown),
      auto_connect_(false),
      check_portal_(kCheckPortalAuto),
      connectable_(false),
      explicitly_disconnected_(false),
      favorite_(false),
      priority_(kPriorityNone),
      security_level_(0),
      strength_(0),
      save_credentials_(true),
      technology_(technology),
      dispatcher_(dispatcher),
      unique_name_(base::UintToString(serial_number_++)),
      friendly_name_(unique_name_),
      available_(false),
      configured_(false),
      configuration_(NULL),
      adaptor_(control_interface->CreateServiceAdaptor(this)),
      metrics_(metrics),
      manager_(manager) {

  HelpRegisterDerivedBool(flimflam::kAutoConnectProperty,
                          &Service::GetAutoConnect,
                          &Service::SetAutoConnect);

  // flimflam::kActivationStateProperty: Registered in CellularService
  // flimflam::kCellularApnProperty: Registered in CellularService
  // flimflam::kCellularLastGoodApnProperty: Registered in CellularService
  // flimflam::kNetworkTechnologyProperty: Registered in CellularService
  // flimflam::kOperatorNameProperty: DEPRECATED
  // flimflam::kOperatorCodeProperty: DEPRECATED
  // flimflam::kRoamingStateProperty: Registered in CellularService
  // flimflam::kServingOperatorProperty: Registered in CellularService
  // flimflam::kPaymentURLProperty: Registered in CellularService

  store_.RegisterString(flimflam::kCheckPortalProperty, &check_portal_);
  store_.RegisterConstBool(flimflam::kConnectableProperty, &connectable_);
  HelpRegisterDerivedString(flimflam::kDeviceProperty,
                            &Service::GetDeviceRpcId,
                            NULL);

  store_.RegisterString(flimflam::kEapIdentityProperty, &eap_.identity);
  store_.RegisterString(flimflam::kEAPEAPProperty, &eap_.eap);
  store_.RegisterString(flimflam::kEapPhase2AuthProperty, &eap_.inner_eap);
  store_.RegisterString(flimflam::kEapAnonymousIdentityProperty,
                        &eap_.anonymous_identity);
  store_.RegisterString(flimflam::kEAPClientCertProperty, &eap_.client_cert);
  store_.RegisterString(flimflam::kEAPCertIDProperty, &eap_.cert_id);
  store_.RegisterString(flimflam::kEapPrivateKeyProperty, &eap_.private_key);
  HelpRegisterDerivedString(flimflam::kEapPrivateKeyPasswordProperty, NULL,
                            &Service::SetEAPPrivateKeyPassword);
  store_.RegisterString(flimflam::kEAPKeyIDProperty, &eap_.key_id);
  store_.RegisterString(flimflam::kEapCaCertProperty, &eap_.ca_cert);
  store_.RegisterString(flimflam::kEapCaCertIDProperty, &eap_.ca_cert_id);
  store_.RegisterString(flimflam::kEAPPINProperty, &eap_.pin);
  HelpRegisterDerivedString(flimflam::kEapPasswordProperty, NULL,
                            &Service::SetEAPPassword);
  store_.RegisterString(flimflam::kEapKeyMgmtProperty, &eap_.key_management);
  store_.RegisterBool(flimflam::kEapUseSystemCAsProperty, &eap_.use_system_cas);

  store_.RegisterConstString(flimflam::kErrorProperty, &error_);
  store_.RegisterConstBool(flimflam::kFavoriteProperty, &favorite_);
  HelpRegisterDerivedUint16(shill::kHTTPProxyPortProperty,
                            &Service::GetHTTPProxyPort,
                            NULL);
  HelpRegisterDerivedBool(flimflam::kIsActiveProperty,
                          &Service::IsActive,
                          NULL);
  // flimflam::kModeProperty: Registered in WiFiService
  store_.RegisterConstString(flimflam::kNameProperty, &friendly_name_);
  // flimflam::kPassphraseProperty: Registered in WiFiService
  // flimflam::kPassphraseRequiredProperty: Registered in WiFiService
  store_.RegisterInt32(flimflam::kPriorityProperty, &priority_);
  HelpRegisterDerivedString(flimflam::kProfileProperty,
                            &Service::GetProfileRpcId,
                            &Service::SetProfileRpcId);
  store_.RegisterString(flimflam::kProxyConfigProperty, &proxy_config_);
  // TODO(cmasone): Create VPN Service with this property
  // store_.RegisterConstStringmap(flimflam::kProviderProperty, &provider_);

  store_.RegisterBool(flimflam::kSaveCredentialsProperty, &save_credentials_);
  HelpRegisterDerivedString(flimflam::kTypeProperty,
                            &Service::GetTechnologyString,
                            NULL);
  // flimflam::kSecurityProperty: Registered in WiFiService
  HelpRegisterDerivedString(flimflam::kStateProperty,
                            &Service::CalculateState,
                            NULL);
  store_.RegisterConstUint8(flimflam::kSignalStrengthProperty, &strength_);
  store_.RegisterString(flimflam::kUIDataProperty, &ui_data_);
  // flimflam::kWifiAuthMode: Registered in WiFiService
  // flimflam::kWifiHiddenSsid: Registered in WiFiService
  // flimflam::kWifiFrequency: Registered in WiFiService
  // flimflam::kWifiPhyMode: Registered in WiFiService
  // flimflam::kWifiHexSsid: Registered in WiFiService

  metrics_->RegisterService(this);

  VLOG(2) << "Service initialized.";
}

Service::~Service() {
  metrics_->DeregisterService(this);
}

void Service::AutoConnect() {
  if (this->IsAutoConnectable()) {
    Error error;
    Connect(&error);
  } else {
    LOG(INFO) << "Suppressed autoconnect to " << friendly_name();
  }
}

void Service::Connect(Error */*error*/) {
  explicitly_disconnected_ = false;
}

void Service::Disconnect(Error */*error*/) {
  explicitly_disconnected_ = true;
}

void Service::ActivateCellularModem(const string &/*carrier*/,
                                    ReturnerInterface *returner) {
  Error error;
  Error::PopulateAndLog( &error, Error::kNotSupported,
                         "Service doesn't support cellular modem activation.");
  returner->ReturnError(error);
}

bool Service::TechnologyIs(const Technology::Identifier /*type*/) const {
  return false;
}

bool Service::IsActive(Error */*error*/) {
  return state_ != kStateUnknown &&
    state_ != kStateIdle &&
    state_ != kStateFailure;
}

void Service::SetState(ConnectState state) {
  LOG(INFO) << "In " << __func__ << "(): Service " << friendly_name_
            << " state " << ConnectStateToString(state_) << " -> "
            << ConnectStateToString(state);

  if (state == state_) {
    return;
  }

  state_ = state;
  if (state != kStateFailure) {
    failure_ = kFailureUnknown;
  }
  manager_->UpdateService(this);
  metrics_->NotifyServiceStateChanged(this, state);
  Error error;
  if (state == kStateConnected) {
    // TODO(quiche): After we have portal detection in place, CalculateState
    // should map kStateConnected to kStateReady. At that point, we should
    // remove this. crosbug.com/23318.
    adaptor_->EmitStringChanged(
        flimflam::kStateProperty, flimflam::kStateReady);
  }
  adaptor_->EmitStringChanged(flimflam::kStateProperty, CalculateState(&error));
}

void Service::SetFailure(ConnectFailure failure) {
  failure_ = failure;
  SetState(kStateFailure);
}

string Service::GetRpcIdentifier() const {
  return adaptor_->GetRpcIdentifier();
}

bool Service::IsLoadableFrom(StoreInterface *storage) const {
  return storage->ContainsGroup(GetStorageIdentifier());
}

bool Service::Load(StoreInterface *storage) {
  const string id = GetStorageIdentifier();
  if (!storage->ContainsGroup(id)) {
    LOG(WARNING) << "Service is not available in the persistent store: " << id;
    return false;
  }
  storage->GetBool(id, kStorageAutoConnect, &auto_connect_);
  storage->GetString(id, kStorageCheckPortal, &check_portal_);
  storage->GetBool(id, kStorageFavorite, &favorite_);
  storage->GetInt(id, kStoragePriority, &priority_);
  storage->GetString(id, kStorageProxyConfig, &proxy_config_);
  storage->GetBool(id, kStorageSaveCredentials, &save_credentials_);
  storage->GetString(id, kStorageUIData, &ui_data_);

  LoadEapCredentials(storage, id);

  // TODO(petkov): Load these:

  // "Failure"
  // "Modified"
  // "LastAttempt"
  // "APN"
  // "LastGoodAPN"

  explicitly_disconnected_ = false;
  favorite_ = true;

  return true;
}

void Service::Unload() {
  auto_connect_ = false;
  check_portal_ = kCheckPortalAuto;
  favorite_ = false;
  priority_ = kPriorityNone;
  proxy_config_ = "";
  save_credentials_ = true;
  ui_data_ = "";

  UnloadEapCredentials();
}

bool Service::Save(StoreInterface *storage) {
  const string id = GetStorageIdentifier();

  storage->SetString(id, kStorageType, GetTechnologyString(NULL));

  // TODO(petkov): We could choose to simplify the saving code by removing most
  // conditionals thus saving even default values.
  if (favorite_) {
    storage->SetBool(id, kStorageAutoConnect, auto_connect_);
  }
  if (check_portal_ == kCheckPortalAuto) {
    storage->DeleteKey(id, kStorageCheckPortal);
  } else {
    storage->SetString(id, kStorageCheckPortal, check_portal_);
  }
  storage->SetBool(id, kStorageFavorite, favorite_);
  storage->SetString(id, kStorageName, friendly_name_);
  if (priority_ != kPriorityNone) {
    storage->SetInt(id, kStoragePriority, priority_);
  } else {
    storage->DeleteKey(id, kStoragePriority);
  }
  SaveString(storage, id, kStorageProxyConfig, proxy_config_, false, true);
  if (save_credentials_) {
    storage->DeleteKey(id, kStorageSaveCredentials);
  } else {
    storage->SetBool(id, kStorageSaveCredentials, false);
  }
  SaveString(storage, id, kStorageUIData, ui_data_, false, true);

  SaveEapCredentials(storage, id);

  // TODO(petkov): Save these:

  // "WiFi.HiddenSSID"
  // "SSID"
  // "Failure"
  // "Modified"
  // "LastAttempt"
  // WiFiService: "Passphrase"
  // "APN"
  // "LastGoodAPN"

  return true;
}

void Service::MakeFavorite() {
  if (favorite_) {
    // We do not want to clobber the value of auto_connect_ (it may
    // be user-set). So return early.
    return;
  }

  auto_connect_ = true;
  favorite_ = true;
}

void Service::SetConnection(ConnectionRefPtr connection) {
  if (connection.get()) {
    http_proxy_.reset(new HTTPProxy(connection));
    http_proxy_->Start(dispatcher_, &sockets_);
  } else {
    http_proxy_.reset();
  }
  connection_ = connection;
}

bool Service::Is8021xConnectable() const {
  // We mirror all the flimflam checks (see service.c:is_connectable()).

  // Identity is required.
  if (eap_.identity.empty()) {
    VLOG(2) << "Not connectable: Identity is empty.";
    return false;
  }

  if (!eap_.client_cert.empty() || !eap_.cert_id.empty()) {
    // If a client certificate is being used, we must have a private key.
    if (eap_.private_key.empty() && eap_.key_id.empty()) {
      VLOG(2) << "Not connectable. Client certificate but no private key.";
      return false;
    }
  }
  if (!eap_.cert_id.empty() || !eap_.key_id.empty() ||
      !eap_.ca_cert_id.empty()) {
    // If PKCS#11 data is needed, a PIN is required.
    if (eap_.pin.empty()) {
      VLOG(2) << "Not connectable. PKCS#11 data but no PIN.";
      return false;
    }
  }

  // For EAP-TLS, a client certificate is required.
  if (eap_.eap.empty() || eap_.eap == "TLS") {
    if (!eap_.client_cert.empty() || !eap_.cert_id.empty()) {
      VLOG(2) << "Connectable. EAP-TLS with a client cert.";
      return true;
    }
  }

  // For EAP types other than TLS (e.g. EAP-TTLS or EAP-PEAP, password is the
  // minimum requirement), at least an identity + password is required.
  if (eap_.eap.empty() || eap_.eap != "TLS") {
    if (!eap_.password.empty()) {
      VLOG(2) << "Connectable. !EAP-TLS and has a password.";
      return true;
    }
  }

  VLOG(2) << "Not connectable. No suitable EAP configuration was found.";
  return false;
}

void Service::set_eap(const EapCredentials &eap) {
  eap_ = eap;
  // Note: Connectability can only be updated by a subclass of Service
  // with knowledge of whether the service actually uses 802.1x credentials.
}

// static
const char *Service::ConnectFailureToString(const ConnectFailure &state) {
  switch (state) {
    case kFailureUnknown:
      return "Unknown";
    case kFailureActivationFailure:
      return "Activation Failure";
    case kFailureOutOfRange:
      return "Out of range";
    case kFailurePinMissing:
      return "PIN missing";
    case kFailureConfigurationFailed:
      return "Configuration Failed";
    case kFailureBadCredentials:
      return "Bad Credentials";
    case kFailureNeedEVDO:
      return "Need EVDO";
    case kFailureNeedHomeNetwork:
      return "Need Home Network";
    case kFailureOTASPFailure:
      return "OTASP Failure";
    case kFailureAAAFailure:
      return "AAA Failure";
    case kFailureMax:
      return "Max failure error code";
  }
  return "Invalid";
}

// static
const char *Service::ConnectStateToString(const ConnectState &state) {
  switch (state) {
    case kStateUnknown:
      return "Unknown";
    case kStateIdle:
      return "Idle";
    case kStateAssociating:
      return "Associating";
    case kStateConfiguring:
      return "Configuring";
    case kStateConnected:
      return "Connected";
    case kStateDisconnected:
      return "Disconnected";
    case kStateReady:
      return "Ready";
    case kStatePortal:
      return "Portal";
    case kStateFailure:
      return "Failure";
    case kStateOnline:
      return "Online";
  }
  return "Invalid";
}

string Service::GetTechnologyString(Error */*error*/) {
  return Technology::NameFromIdentifier(technology());
}

// static
bool Service::DecideBetween(int a, int b, bool *decision) {
  if (a == b)
    return false;
  *decision = (a > b);
  return true;
}

// static
bool Service::Compare(ServiceRefPtr a,
                      ServiceRefPtr b,
                      const vector<Technology::Identifier> &tech_order) {
  bool ret;

  if (a->state() != b->state()) {
    if (DecideBetween(a->IsConnected(), b->IsConnected(), &ret)) {
      return ret;
    }

    // TODO(pstew): Services don't know about portal state yet

    if (DecideBetween(a->IsConnecting(), b->IsConnecting(), &ret)) {
      return ret;
    }

    if (DecideBetween(!a->IsFailed(), !b->IsFailed(), &ret)) {
      return ret;
    }
  }

  if (DecideBetween(a->connectable(), b->connectable(), &ret) ||
      DecideBetween(a->auto_connect(), b->auto_connect(), &ret) ||
      DecideBetween(a->favorite(), b->favorite(), &ret) ||
      DecideBetween(a->priority(), b->priority(), &ret)) {
    return ret;
  }

  // TODO(pstew): Below this point we are making value judgements on
  // services that are not related to anything intrinsic or
  // user-specified.  These heuristics should be richer (contain
  // historical information, for example) and be subject to user
  // customization.

  for (vector<Technology::Identifier>::const_iterator it = tech_order.begin();
       it != tech_order.end();
       ++it) {
    if (DecideBetween(a->TechnologyIs(*it), b->TechnologyIs(*it), &ret))
      return ret;
  }

  if (DecideBetween(a->security_level(), b->security_level(), &ret) ||
      DecideBetween(a->strength(), b->strength(), &ret)) {
    return ret;
  }

  return a->UniqueName() < b->UniqueName();
}

const ProfileRefPtr &Service::profile() const { return profile_; }

void Service::set_profile(const ProfileRefPtr &p) { profile_ = p; }

void Service::set_connectable(bool connectable) {
  connectable_ = connectable;
  adaptor_->EmitBoolChanged(flimflam::kConnectableProperty, connectable_);
}

string Service::CalculateState(Error */*error*/) {
  switch (state_) {
    case kStateIdle:
      return flimflam::kStateIdle;
    case kStateAssociating:
      return flimflam::kStateAssociation;
    case kStateConfiguring:
      return flimflam::kStateConfiguration;
    case kStateConnected:
      // TODO(gauravsh): Until portal handling is implemented, go to "online"
      // instead of "ready" state. crosbug.com/23318
      return flimflam::kStateOnline;
    case kStateDisconnected:
      return flimflam::kStateDisconnect;
    case kStateFailure:
      return flimflam::kStateFailure;
    case kStateOnline:
      return flimflam::kStateOnline;
    case kStateUnknown:
    default:
      return "";
  }
}

bool Service::IsAutoConnectable() const {
  return connectable() && !IsConnected() && !IsConnecting() &&
      !explicitly_disconnected_;
}

void Service::HelpRegisterDerivedBool(
    const string &name,
    bool(Service::*get)(Error *),
    void(Service::*set)(const bool&, Error *)) {
  store_.RegisterDerivedBool(
      name,
      BoolAccessor(new CustomAccessor<Service, bool>(this, get, set)));
}

void Service::HelpRegisterDerivedString(
    const string &name,
    string(Service::*get)(Error *),
    void(Service::*set)(const string&, Error *)) {
  store_.RegisterDerivedString(
      name,
      StringAccessor(new CustomAccessor<Service, string>(this, get, set)));
}

void Service::HelpRegisterDerivedUint16(
    const string &name,
    uint16(Service::*get)(Error *),
    void(Service::*set)(const uint16&, Error *)) {
  store_.RegisterDerivedUint16(
      name,
      Uint16Accessor(new CustomAccessor<Service, uint16>(this, get, set)));
}

void Service::SaveString(StoreInterface *storage,
                         const string &id,
                         const string &key,
                         const string &value,
                         bool crypted,
                         bool save) {
  if (value.empty() || !save) {
    storage->DeleteKey(id, key);
    return;
  }
  if (crypted) {
    storage->SetCryptedString(id, key, value);
    return;
  }
  storage->SetString(id, key, value);
}

void Service::LoadEapCredentials(StoreInterface *storage, const string &id) {
  EapCredentials eap;
  storage->GetCryptedString(id, kStorageEapIdentity, &eap.identity);
  storage->GetString(id, kStorageEapEap, &eap.eap);
  storage->GetString(id, kStorageEapInnerEap, &eap.inner_eap);
  storage->GetCryptedString(id,
                            kStorageEapAnonymousIdentity,
                            &eap.anonymous_identity);
  storage->GetString(id, kStorageEapClientCert, &eap.client_cert);
  storage->GetString(id, kStorageEapCertID, &eap.cert_id);
  storage->GetString(id, kStorageEapPrivateKey, &eap.private_key);
  storage->GetCryptedString(id,
                            kStorageEapPrivateKeyPassword,
                            &eap.private_key_password);
  storage->GetString(id, kStorageEapKeyID, &eap.key_id);
  storage->GetString(id, kStorageEapCACert, &eap.ca_cert);
  storage->GetString(id, kStorageEapCACertID, &eap.ca_cert_id);
  storage->GetBool(id, kStorageEapUseSystemCAs, &eap.use_system_cas);
  storage->GetString(id, kStorageEapPIN, &eap.pin);
  storage->GetCryptedString(id, kStorageEapPassword, &eap.password);
  storage->GetString(id, kStorageEapKeyManagement, &eap.key_management);
  set_eap(eap);
}

void Service::SaveEapCredentials(StoreInterface *storage, const string &id) {
  bool save = save_credentials_;
  SaveString(storage, id, kStorageEapIdentity, eap_.identity, true, save);
  SaveString(storage, id, kStorageEapEap, eap_.eap, false, true);
  SaveString(storage, id, kStorageEapInnerEap, eap_.inner_eap, false, true);
  SaveString(storage,
             id,
             kStorageEapAnonymousIdentity,
             eap_.anonymous_identity,
             true,
             save);
  SaveString(storage, id, kStorageEapClientCert, eap_.client_cert, false, save);
  SaveString(storage, id, kStorageEapCertID, eap_.cert_id, false, save);
  SaveString(storage, id, kStorageEapPrivateKey, eap_.private_key, false, save);
  SaveString(storage,
             id,
             kStorageEapPrivateKeyPassword,
             eap_.private_key_password,
             true,
             save);
  SaveString(storage, id, kStorageEapKeyID, eap_.key_id, false, save);
  SaveString(storage, id, kStorageEapCACert, eap_.ca_cert, false, true);
  SaveString(storage, id, kStorageEapCACertID, eap_.ca_cert_id, false, true);
  storage->SetBool(id, kStorageEapUseSystemCAs, eap_.use_system_cas);
  SaveString(storage, id, kStorageEapPIN, eap_.pin, false, save);
  SaveString(storage, id, kStorageEapPassword, eap_.password, true, save);
  SaveString(storage,
             id,
             kStorageEapKeyManagement,
             eap_.key_management,
             false,
             true);
}

void Service::UnloadEapCredentials() {
  eap_.identity = "";
  eap_.eap = "";
  eap_.inner_eap = "";
  eap_.anonymous_identity = "";
  eap_.client_cert = "";
  eap_.cert_id = "";
  eap_.private_key = "";
  eap_.private_key_password = "";
  eap_.key_id = "";
  eap_.ca_cert = "";
  eap_.ca_cert_id = "";
  eap_.use_system_cas = false;
  eap_.pin = "";
  eap_.password = "";
  eap_.key_management = "";
}

const string &Service::GetEAPKeyManagement() const {
  return eap_.key_management;
}

void Service::SetEAPKeyManagement(const string &key_management) {
  eap_.key_management = key_management;
}

bool Service::GetAutoConnect(Error */*error*/) {
  return auto_connect();
}

void Service::SetAutoConnect(const bool &connect, Error *error) {
  if (favorite_) {
    set_auto_connect(connect);
  } else {
    error->Populate(Error::kInvalidArguments, "Property is read-only");
  }
}

void Service::SetEAPPassword(const string &password, Error */*error*/) {
  eap_.password = password;
}

void Service::SetEAPPrivateKeyPassword(const string &password,
                                       Error */*error*/) {
  eap_.private_key_password = password;
}

string Service::GetProfileRpcId(Error *error) {
  if (!profile_) {
    // This happens in some unit tests where profile_ is not set.
    error->Populate(Error::kNotFound);
    return "";
  }
  return profile_->GetRpcIdentifier();
}

void Service::SetProfileRpcId(const string &profile, Error *error) {
  manager_->SetProfileForService(this, profile, error);
}

uint16 Service::GetHTTPProxyPort(Error */*error*/) {
  if (http_proxy_.get()) {
    return static_cast<uint16>(http_proxy_->proxy_port());
  }
  return 0;
}

void Service::SetStrength(uint8 strength) {
  if (strength == strength_) {
    return;
  }
  strength_ = strength;
  adaptor_->EmitUint8Changed(flimflam::kSignalStrengthProperty, strength);
}

}  // namespace shill
