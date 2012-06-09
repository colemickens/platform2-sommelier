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
#include <base/stringprintf.h>
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
#include "shill/scope_logger.h"
#include "shill/service_dbus_adaptor.h"
#include "shill/sockets.h"
#include "shill/store_interface.h"

using std::map;
using std::string;
using std::vector;

namespace shill {

const char Service::kAutoConnConnected[] = "connected";
const char Service::kAutoConnConnecting[] = "connecting";
const char Service::kAutoConnExplicitDisconnect[] = "explicitly disconnected";
const char Service::kAutoConnNotConnectable[] = "not connectable";

const size_t Service::kEAPMaxCertificationElements = 10;

const char Service::kCheckPortalAuto[] = "auto";
const char Service::kCheckPortalFalse[] = "false";
const char Service::kCheckPortalTrue[] = "true";

const int Service::kPriorityNone = 0;

const char Service::kServiceSortAutoConnect[] = "AutoConnect";
const char Service::kServiceSortConnectable[] = "Connectable";
const char Service::kServiceSortFavorite[] = "Favorite";
const char Service::kServiceSortIsConnected[] = "IsConnected";
const char Service::kServiceSortIsConnecting[] = "IsConnecting";
const char Service::kServiceSortIsFailed[] = "IsFailed";
const char Service::kServiceSortIsPortalled[] = "IsPortal";
const char Service::kServiceSortPriority[] = "Priority";
const char Service::kServiceSortSecurityEtc[] = "SecurityEtc";
const char Service::kServiceSortTechnology[] = "Technology";
const char Service::kServiceSortUniqueName[] = "UniqueName";

const char Service::kStorageAutoConnect[] = "AutoConnect";
const char Service::kStorageCheckPortal[] = "CheckPortal";
const char Service::kStorageEapAnonymousIdentity[] = "EAP.AnonymousIdentity";
const char Service::kStorageEapCACert[] = "EAP.CACert";
const char Service::kStorageEapCACertID[] = "EAP.CACertID";
const char Service::kStorageEapCACertNSS[] = "EAP.CACertNSS";
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
const char Service::kStorageError[] = "Error";
const char Service::kStorageFavorite[] = "Favorite";
const char Service::kStorageGUID[] = "GUID";
const char Service::kStorageHasEverConnected[] = "HasEverConnected";
const char Service::kStorageName[] = "Name";
const char Service::kStoragePriority[] = "Priority";
const char Service::kStorageProxyConfig[] = "ProxyConfig";
const char Service::kStorageSaveCredentials[] = "SaveCredentials";
const char Service::kStorageType[] = "Type";
const char Service::kStorageUIData[] = "UIData";

const uint8 Service::kStrengthMax = 100;
const uint8 Service::kStrengthMin = 0;

// static
unsigned int Service::serial_number_ = 0;

Service::Service(ControlInterface *control_interface,
                 EventDispatcher *dispatcher,
                 Metrics *metrics,
                 Manager *manager,
                 Technology::Identifier technology)
    : state_(kStateIdle),
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
      failed_time_(0),
      has_ever_connected_(false),
      dispatcher_(dispatcher),
      unique_name_(base::UintToString(serial_number_++)),
      friendly_name_(unique_name_),
      available_(false),
      configured_(false),
      configuration_(NULL),
      adaptor_(control_interface->CreateServiceAdaptor(this)),
      metrics_(metrics),
      manager_(manager),
      sockets_(new Sockets()) {
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

  HelpRegisterDerivedString(flimflam::kCheckPortalProperty,
                            &Service::GetCheckPortal,
                            &Service::SetCheckPortal);
  store_.RegisterConstBool(flimflam::kConnectableProperty, &connectable_);
  HelpRegisterDerivedRpcIdentifier(flimflam::kDeviceProperty,
                                   &Service::GetDeviceRpcId,
                                   NULL);
  store_.RegisterString(flimflam::kGuidProperty, &guid_);

  store_.RegisterString(flimflam::kEapIdentityProperty, &eap_.identity);
  store_.RegisterString(flimflam::kEAPEAPProperty, &eap_.eap);
  store_.RegisterString(flimflam::kEapPhase2AuthProperty, &eap_.inner_eap);
  store_.RegisterString(flimflam::kEapAnonymousIdentityProperty,
                        &eap_.anonymous_identity);
  store_.RegisterString(flimflam::kEAPClientCertProperty, &eap_.client_cert);
  store_.RegisterString(flimflam::kEAPCertIDProperty, &eap_.cert_id);
  store_.RegisterString(flimflam::kEapPrivateKeyProperty, &eap_.private_key);
  HelpRegisterWriteOnlyDerivedString(flimflam::kEapPrivateKeyPasswordProperty,
                                     &Service::SetEAPPrivateKeyPassword,
                                     NULL,
                                     &eap_.private_key_password);
  store_.RegisterString(flimflam::kEAPKeyIDProperty, &eap_.key_id);
  store_.RegisterString(flimflam::kEapCaCertProperty, &eap_.ca_cert);
  store_.RegisterString(flimflam::kEapCaCertIDProperty, &eap_.ca_cert_id);
  store_.RegisterString(flimflam::kEapCaCertNssProperty, &eap_.ca_cert_nss);
  store_.RegisterString(flimflam::kEAPPINProperty, &eap_.pin);
  HelpRegisterWriteOnlyDerivedString(flimflam::kEapPasswordProperty,
                                     &Service::SetEAPPassword,
                                     NULL,
                                     &eap_.password);
  store_.RegisterString(flimflam::kEapKeyMgmtProperty, &eap_.key_management);
  store_.RegisterBool(flimflam::kEapUseSystemCAsProperty, &eap_.use_system_cas);
  store_.RegisterConstStrings(shill::kEapRemoteCertificationProperty,
                              &eap_.remote_certification);
  store_.RegisterString(shill::kEapSubjectMatchProperty,
                        &eap_.subject_match);

  // TODO(ers): in flimflam clearing Error has the side-effect of
  // setting the service state to IDLE. Is this important? I could
  // see an autotest depending on it.
  store_.RegisterConstString(flimflam::kErrorProperty, &error_);
  store_.RegisterConstBool(flimflam::kFavoriteProperty, &favorite_);
  HelpRegisterDerivedUint16(shill::kHTTPProxyPortProperty,
                            &Service::GetHTTPProxyPort,
                            NULL);
  HelpRegisterDerivedRpcIdentifier(shill::kIPConfigProperty,
                                   &Service::GetIPConfigRpcIdentifier,
                                   NULL);
  HelpRegisterDerivedBool(flimflam::kIsActiveProperty,
                          &Service::IsActive,
                          NULL);
  // flimflam::kModeProperty: Registered in WiFiService

  // Although this is a read-only property, some callers want to blindly
  // set this value to its current value.
  HelpRegisterDerivedString(flimflam::kNameProperty,
                            &Service::GetNameProperty,
                            &Service::AssertTrivialSetNameProperty);
  // flimflam::kPassphraseProperty: Registered in WiFiService
  // flimflam::kPassphraseRequiredProperty: Registered in WiFiService
  store_.RegisterInt32(flimflam::kPriorityProperty, &priority_);
  HelpRegisterDerivedString(flimflam::kProfileProperty,
                            &Service::GetProfileRpcId,
                            &Service::SetProfileRpcId);
  store_.RegisterString(flimflam::kProxyConfigProperty, &proxy_config_);
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

  static_ip_parameters_.PlumbPropertyStore(&store_);

  IgnoreParameterForConfigure(flimflam::kTypeProperty);
  IgnoreParameterForConfigure(flimflam::kProfileProperty);

  SLOG(Service, 2) << "Service initialized.";
}

Service::~Service() {
  metrics_->DeregisterService(this);
}

void Service::AutoConnect() {
  const char *reason;
  if (this->IsAutoConnectable(&reason)) {
    Error error;
    Connect(&error);
  } else {
    LOG(INFO) << "Suppressed autoconnect to " << friendly_name() << " "
              << "(" << reason << ")";
  }
}

void Service::Connect(Error */*error*/) {
  explicitly_disconnected_ = false;
  // clear any failure state from a previous connect attempt
  SetState(kStateIdle);
}

void Service::Disconnect(Error */*error*/) {
  explicitly_disconnected_ = true;
}

void Service::ActivateCellularModem(const string &/*carrier*/,
                                    Error *error,
                                    const ResultCallback &/*callback*/) {
  Error::PopulateAndLog(error, Error::kNotSupported,
                        "Service doesn't support cellular modem activation.");
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
    failed_time_ = 0;
  }
  if (state == kStateConnected) {
    has_ever_connected_ = true;
    SaveToProfile();
  }
  manager_->UpdateService(this);
  metrics_->NotifyServiceStateChanged(this, state);
  Error error;
  adaptor_->EmitStringChanged(flimflam::kStateProperty, CalculateState(&error));
}

void Service::SetFailure(ConnectFailure failure) {
  failure_ = failure;
  failed_time_ = time(NULL);
  SetState(kStateFailure);
}

void Service::SetFailureSilent(ConnectFailure failure) {
  // Note that order matters here, since SetState modifies |failure_| and
  // |failed_time_|.
  SetState(kStateIdle);
  failure_ = failure;
  failed_time_ = time(NULL);
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
  storage->GetString(id, kStorageGUID, &guid_);
  storage->GetBool(id, kStorageHasEverConnected, &has_ever_connected_);
  storage->GetInt(id, kStoragePriority, &priority_);
  storage->GetString(id, kStorageProxyConfig, &proxy_config_);
  storage->GetBool(id, kStorageSaveCredentials, &save_credentials_);
  storage->GetString(id, kStorageUIData, &ui_data_);

  LoadEapCredentials(storage, id);
  static_ip_parameters_.Load(storage, id);
  // TODO(petkov): Load these:

  // "Failure"
  // "Modified"
  // "LastAttempt"

  explicitly_disconnected_ = false;
  favorite_ = true;

  return true;
}

bool Service::Unload() {
  auto_connect_ = false;
  check_portal_ = kCheckPortalAuto;
  favorite_ = false;
  priority_ = kPriorityNone;
  proxy_config_ = "";
  save_credentials_ = true;
  ui_data_ = "";

  UnloadEapCredentials();
  Error error;  // Ignored.
  Disconnect(&error);
  return false;
}

bool Service::Save(StoreInterface *storage) {
  const string id = GetStorageIdentifier();

  storage->SetString(id, kStorageType, GetTechnologyString(NULL));

  // TODO(petkov): We could choose to simplify the saving code by removing most
  // conditionals thus saving even default values.
  storage->SetBool(id, kStorageAutoConnect, auto_connect_);
  if (check_portal_ == kCheckPortalAuto) {
    storage->DeleteKey(id, kStorageCheckPortal);
  } else {
    storage->SetString(id, kStorageCheckPortal, check_portal_);
  }
  storage->SetBool(id, kStorageFavorite, favorite_);
  SaveString(storage, id, kStorageGUID, guid_, false, true);
  storage->SetBool(id, kStorageHasEverConnected, has_ever_connected_);
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
  static_ip_parameters_.Save(storage, id);

  // TODO(petkov): Save these:

  // "Failure"
  // "Modified"
  // "LastAttempt"

  return true;
}

void Service::SaveToCurrentProfile() {
  // Some unittests do not specify a manager.
  if (manager()) {
    manager()->SaveServiceToProfile(this);
  }
}

void Service::Configure(const KeyValueStore &args, Error *error) {
  map<string, bool>::const_iterator bool_it;
  SLOG(Service, 5) << "Configuring bool properties:";
  for (bool_it = args.bool_properties().begin();
       bool_it != args.bool_properties().end();
       ++bool_it) {
    if (ContainsKey(parameters_ignored_for_configure_, bool_it->first)) {
      continue;
    }
    SLOG(Service, 5) << "   " << bool_it->first;
    Error set_error;
    store_.SetBoolProperty(bool_it->first, bool_it->second, &set_error);
    if (error->IsSuccess() && set_error.IsFailure()) {
      error->CopyFrom(set_error);
    }
  }
  SLOG(Service, 5) << "Configuring string properties:";
  map<string, string>::const_iterator string_it;
  for (string_it = args.string_properties().begin();
       string_it != args.string_properties().end();
       ++string_it) {
    if (ContainsKey(parameters_ignored_for_configure_, string_it->first)) {
      continue;
    }
    SLOG(Service, 5) << "   " << string_it->first;
    Error set_error;
    store_.SetStringProperty(string_it->first, string_it->second, &set_error);
    if (error->IsSuccess() && set_error.IsFailure()) {
      error->CopyFrom(set_error);
    }
  }
  SLOG(Service, 5) << "Configuring uint32 properties:";
  map<string, uint32>::const_iterator int_it;
  for (int_it = args.uint_properties().begin();
       int_it != args.uint_properties().end();
       ++int_it) {
    if (ContainsKey(parameters_ignored_for_configure_, int_it->first)) {
      continue;
    }
    SLOG(Service, 5) << "   " << int_it->first;
    Error set_error;
    store_.SetUint32Property(int_it->first, int_it->second, &set_error);
    if (error->IsSuccess() && set_error.IsFailure()) {
      error->CopyFrom(set_error);
    }
  }
}

bool Service::IsRemembered() const {
  return profile_ && !manager_->IsServiceEphemeral(this);
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

void Service::SetConnection(const ConnectionRefPtr &connection) {
  if (connection.get()) {
    http_proxy_.reset(new HTTPProxy(connection));
    http_proxy_->Start(dispatcher_, sockets_.get());
  } else {
    http_proxy_.reset();
  }
  connection_ = connection;
}

bool Service::Is8021xConnectable() const {
  // We mirror all the flimflam checks (see service.c:is_connectable()).

  // Identity is required.
  if (eap_.identity.empty()) {
    SLOG(Service, 2) << "Not connectable: Identity is empty.";
    return false;
  }

  if (!eap_.client_cert.empty() || !eap_.cert_id.empty()) {
    // If a client certificate is being used, we must have a private key.
    if (eap_.private_key.empty() && eap_.key_id.empty()) {
      SLOG(Service, 2)
          << "Not connectable. Client certificate but no private key.";
      return false;
    }
  }
  if (!eap_.cert_id.empty() || !eap_.key_id.empty() ||
      !eap_.ca_cert_id.empty()) {
    // If PKCS#11 data is needed, a PIN is required.
    if (eap_.pin.empty()) {
      SLOG(Service, 2) << "Not connectable. PKCS#11 data but no PIN.";
      return false;
    }
  }

  // For EAP-TLS, a client certificate is required.
  if (eap_.eap.empty() || eap_.eap == "TLS") {
    if ((!eap_.client_cert.empty() || !eap_.cert_id.empty()) &&
        (!eap_.private_key.empty() || !eap_.key_id.empty())) {
      SLOG(Service, 2) << "Connectable. EAP-TLS with a client cert and key.";
      return true;
    }
  }

  // For EAP types other than TLS (e.g. EAP-TTLS or EAP-PEAP, password is the
  // minimum requirement), at least an identity + password is required.
  if (eap_.eap.empty() || eap_.eap != "TLS") {
    if (!eap_.password.empty()) {
      SLOG(Service, 2) << "Connectable. !EAP-TLS and has a password.";
      return true;
    }
  }

  SLOG(Service, 2)
      << "Not connectable. No suitable EAP configuration was found.";
  return false;
}

bool Service::AddEAPCertification(const string &name, size_t depth) {
  if (depth >= kEAPMaxCertificationElements) {
    LOG(WARNING) << "Ignoring certification " << name
                 << " because depth " << depth
                 << " exceeds our maximum of "
                 << kEAPMaxCertificationElements;
    return false;
  }

  if (depth >= eap_.remote_certification.size()) {
    eap_.remote_certification.resize(depth + 1);
  } else if (name == eap_.remote_certification[depth]) {
    return true;
  }

  eap_.remote_certification[depth] = name;
  LOG(INFO) << "Received certification for "
            << name
            << " at depth "
            << depth;
  return true;
}

void Service::ClearEAPCertification() {
  eap_.remote_certification.clear();
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
                      const vector<Technology::Identifier> &tech_order,
                      const char **reason) {
  bool ret;

  if (a->state() != b->state()) {
    if (DecideBetween(a->IsConnected(), b->IsConnected(), &ret)) {
      *reason = kServiceSortIsConnected;
      return ret;
    }

    if (DecideBetween(!a->IsPortalled(), !b->IsPortalled(), &ret)) {
      *reason = kServiceSortIsPortalled;
      return ret;
    }

    if (DecideBetween(a->IsConnecting(), b->IsConnecting(), &ret)) {
      *reason = kServiceSortIsConnecting;
      return ret;
    }

    if (DecideBetween(!a->IsFailed(), !b->IsFailed(), &ret)) {
      *reason = kServiceSortIsFailed;
      return ret;
    }
  }

  if (DecideBetween(a->connectable(), b->connectable(), &ret)) {
    *reason = kServiceSortConnectable;
    return ret;
  }

  // Ignore the auto-connect property if both services are connected
  // already. This allows connected non-autoconnectable VPN services to be
  // sorted higher than other connected services based on technology order.
  if (!a->IsConnected() &&
      DecideBetween(a->auto_connect(), b->auto_connect(), &ret)) {
    *reason = kServiceSortAutoConnect;
    return ret;
  }

  if (DecideBetween(a->favorite(), b->favorite(), &ret)) {
    *reason = kServiceSortFavorite;
    return ret;
  }

  if (DecideBetween(a->priority(), b->priority(), &ret)) {
    *reason = kServiceSortPriority;
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
    if (DecideBetween(a->technology() == *it, b->technology() == *it, &ret)) {
      *reason = kServiceSortTechnology;
      return ret;
    }
  }

  if (DecideBetween(a->security_level(), b->security_level(), &ret) ||
      DecideBetween(a->strength(), b->strength(), &ret)) {
    *reason = kServiceSortSecurityEtc;
    return ret;
  }

  *reason = kServiceSortUniqueName;
  return a->UniqueName() < b->UniqueName();
}

const ProfileRefPtr &Service::profile() const { return profile_; }

void Service::set_profile(const ProfileRefPtr &p) { profile_ = p; }

void Service::OnPropertyChanged(const string &property) {
  if (Is8021x() &&
      (property == flimflam::kEAPCertIDProperty ||
       property == flimflam::kEAPClientCertProperty ||
       property == flimflam::kEAPKeyIDProperty ||
       property == flimflam::kEAPPINProperty ||
       property == flimflam::kEapCaCertIDProperty ||
       property == flimflam::kEapIdentityProperty ||
       property == flimflam::kEapPasswordProperty ||
       property == flimflam::kEapPrivateKeyProperty)) {
    // This notifies subclassess that EAP parameters have been changed.
    set_eap(eap_);
  }
  SaveToProfile();
  if ((property == flimflam::kCheckPortalProperty ||
       property == flimflam::kProxyConfigProperty) &&
      (state_ == kStateConnected ||
       state_ == kStatePortal ||
       state_ == kStateOnline)) {
    manager_->RecheckPortalOnService(this);
  }
}

string Service::GetIPConfigRpcIdentifier(Error *error) {
  if (!connection_) {
    error->Populate(Error::kNotFound);
    return "/";
  }

  string id = connection_->ipconfig_rpc_identifier();

  if (id.empty()) {
    // Do not return an empty IPConfig.
    error->Populate(Error::kNotFound);
    return "/";
  }

  return id;
}

void Service::set_connectable(bool connectable) {
  connectable_ = connectable;
  adaptor_->EmitBoolChanged(flimflam::kConnectableProperty, connectable_);
}

void Service::SetConnectable(bool connectable) {
  if (connectable_ == connectable) {
    return;
  }
  connectable_ = connectable;
  adaptor_->EmitBoolChanged(flimflam::kConnectableProperty, connectable_);
  if (manager_->HasService(this)) {
    manager_->UpdateService(this);
  }
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
      return flimflam::kStateReady;
    case kStateDisconnected:
      return flimflam::kStateDisconnect;
    case kStateFailure:
      return flimflam::kStateFailure;
    case kStatePortal:
      return flimflam::kStatePortal;
    case kStateOnline:
      return flimflam::kStateOnline;
    case kStateUnknown:
    default:
      return "";
  }
}

bool Service::IsAutoConnectable(const char **reason) const {
  if (!connectable()) {
    *reason = kAutoConnNotConnectable;
    return false;
  }

  if (IsConnected()) {
    *reason = kAutoConnConnected;
    return false;
  }

  if (IsConnecting()) {
    *reason = kAutoConnConnecting;
    return false;
  }

  if (explicitly_disconnected_) {
    *reason = kAutoConnExplicitDisconnect;
    return false;
  }

  return true;
}

bool Service::IsPortalDetectionDisabled() const {
  return check_portal_ == kCheckPortalFalse;
}

bool Service::IsPortalDetectionAuto() const {
  return check_portal_ == kCheckPortalAuto;
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

void Service::HelpRegisterDerivedRpcIdentifier(
    const string &name,
    RpcIdentifier(Service::*get)(Error *),
    void(Service::*set)(const RpcIdentifier&, Error *)) {
  store_.RegisterDerivedRpcIdentifier(
      name,
      RpcIdentifierAccessor(new CustomAccessor<Service, RpcIdentifier>(
          this, get, set)));
}

void Service::HelpRegisterDerivedUint16(
    const string &name,
    uint16(Service::*get)(Error *),
    void(Service::*set)(const uint16&, Error *)) {
  store_.RegisterDerivedUint16(
      name,
      Uint16Accessor(new CustomAccessor<Service, uint16>(this, get, set)));
}

void Service::HelpRegisterWriteOnlyDerivedString(
    const string &name,
    void(Service::*set)(const string &, Error *),
    void(Service::*clear)(Error *),
    const string *default_value) {
  store_.RegisterDerivedString(
      name,
      StringAccessor(
          new CustomWriteOnlyAccessor<Service, string>(
              this, set, clear, default_value)));
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
  storage->GetString(id, kStorageEapCACertNSS, &eap.ca_cert_nss);
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
  SaveString(storage, id, kStorageEapCACertNSS, eap_.ca_cert_nss, false, true);
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
  eap_.use_system_cas = true;
  eap_.pin = "";
  eap_.password = "";
}

void Service::IgnoreParameterForConfigure(const string &parameter) {
  parameters_ignored_for_configure_.insert(parameter);
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
  set_auto_connect(connect);
}

string Service::GetCheckPortal(Error *error) {
  return check_portal_;
}

void Service::SetCheckPortal(const string &check_portal, Error *error) {
  if (check_portal == check_portal_) {
    return;
  }
  if (check_portal != kCheckPortalFalse &&
      check_portal != kCheckPortalTrue &&
      check_portal != kCheckPortalAuto) {
    Error::PopulateAndLog(error, Error::kInvalidArguments,
                          base::StringPrintf(
                              "Invalid Service CheckPortal property value: %s",
                              check_portal.c_str()));
    return;
  }
  check_portal_ = check_portal;
}

void Service::SetEAPPassword(const string &password, Error */*error*/) {
  eap_.password = password;
}

void Service::SetEAPPrivateKeyPassword(const string &password,
                                       Error */*error*/) {
  eap_.private_key_password = password;
}

string Service::GetNameProperty(Error *error) {
  return friendly_name_;
}

void Service::AssertTrivialSetNameProperty(const string &name, Error *error) {
  if (name != friendly_name_) {
    Error::PopulateAndLog(error, Error::kInvalidArguments,
                          base::StringPrintf(
                              "Service Name property cannot be modified "
                              "(%s to %s)", friendly_name_.c_str(),
                              name.c_str()));
  }
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

void Service::SaveToProfile() {
  if (profile_.get() && profile_->GetConstStorage()) {
    profile_->UpdateService(this);
  }
}

void Service::SetStrength(uint8 strength) {
  if (strength == strength_) {
    return;
  }
  strength_ = strength;
  adaptor_->EmitUint8Changed(flimflam::kSignalStrengthProperty, strength);
}

}  // namespace shill
