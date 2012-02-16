// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/wifi_service.h"

#include <string>
#include <utility>

#include <base/logging.h>
#include <base/stringprintf.h>
#include <base/string_number_conversions.h>
#include <base/string_split.h>
#include <base/string_util.h>
#include <chromeos/dbus/service_constants.h>
#include <dbus/dbus.h>

#include "shill/adaptor_interfaces.h"
#include "shill/control_interface.h"
#include "shill/device.h"
#include "shill/error.h"
#include "shill/event_dispatcher.h"
#include "shill/ieee80211.h"
#include "shill/metrics.h"
#include "shill/property_accessor.h"
#include "shill/store_interface.h"
#include "shill/wifi.h"
#include "shill/wifi_endpoint.h"
#include "shill/wpa_supplicant.h"

using std::set;
using std::string;
using std::vector;

namespace shill {

const char WiFiService::kStorageHiddenSSID[] = "WiFi.HiddenSSID";
const char WiFiService::kStorageMode[] = "WiFi.Mode";
const char WiFiService::kStoragePassphrase[] = "Passphrase";
const char WiFiService::kStorageSecurity[] = "WiFi.Security";
const char WiFiService::kStorageSSID[] = "SSID";
bool WiFiService::logged_signal_warning = false;

WiFiService::WiFiService(ControlInterface *control_interface,
                         EventDispatcher *dispatcher,
                         Metrics *metrics,
                         Manager *manager,
                         const WiFiRefPtr &device,
                         const vector<uint8_t> &ssid,
                         const string &mode,
                         const string &security,
                         bool hidden_ssid)
    : Service(control_interface, dispatcher, metrics, manager,
              Technology::kWifi),
      need_passphrase_(false),
      security_(security),
      mode_(mode),
      hidden_ssid_(hidden_ssid),
      frequency_(0),
      physical_mode_(0),
      task_factory_(this),
      wifi_(device),
      ssid_(ssid) {
  PropertyStore *store = this->mutable_store();
  store->RegisterConstString(flimflam::kModeProperty, &mode_);
  HelpRegisterWriteOnlyDerivedString(flimflam::kPassphraseProperty,
                                     &WiFiService::SetPassphrase,
                                     &WiFiService::ClearPassphrase,
                                     NULL);
  store->RegisterBool(flimflam::kPassphraseRequiredProperty, &need_passphrase_);
  store->RegisterConstString(flimflam::kSecurityProperty, &security_);

  store->RegisterConstString(flimflam::kWifiAuthMode, &auth_mode_);
  store->RegisterConstBool(flimflam::kWifiHiddenSsid, &hidden_ssid_);
  store->RegisterConstUint16(flimflam::kWifiFrequency, &frequency_);
  store->RegisterConstUint16(flimflam::kWifiPhyMode, &physical_mode_);

  hex_ssid_ = base::HexEncode(ssid_.data(), ssid_.size());
  string ssid_string(
      reinterpret_cast<const char *>(ssid_.data()), ssid_.size());
  if (WiFi::SanitizeSSID(&ssid_string)) {
    // WifiHexSsid property should only be present if Name property
    // has been munged.
    store->RegisterConstString(flimflam::kWifiHexSsid, &hex_ssid_);
  }
  set_friendly_name(ssid_string);

  // TODO(quiche): determine if it is okay to set EAP.KeyManagement for
  // a service that is not 802.1x.
  if (Is8021x()) {
    // Passphrases are not mandatory for 802.1X.
    need_passphrase_ = false;
  } else if (security_ == flimflam::kSecurityPsk) {
    SetEAPKeyManagement("WPA-PSK");
  } else if (security_ == flimflam::kSecurityRsn) {
    SetEAPKeyManagement("WPA-PSK");
  } else if (security_ == flimflam::kSecurityWpa) {
    SetEAPKeyManagement("WPA-PSK");
  } else if (security_ == flimflam::kSecurityWep) {
    SetEAPKeyManagement("NONE");
  } else if (security_ == flimflam::kSecurityNone) {
    SetEAPKeyManagement("NONE");
  } else {
    LOG(ERROR) << "Unsupported security method " << security_;
  }

  // Until we know better (at Profile load time), use the generic name.
  storage_identifier_ = GetGenericStorageIdentifier();
  UpdateConnectable();
}

WiFiService::~WiFiService() {
  LOG(INFO) << __func__;
}

void WiFiService::AutoConnect() {
  if (IsAutoConnectable()) {
    // Execute immediately, for two reasons:
    //
    // 1. We need IsAutoConnectable to return the correct value for
    //    other WiFiServices, and that depends on WiFi's state.
    //
    // 2. We should probably limit the extent to which we queue up
    //    actions (such as AutoConnect) which depend on current state.
    //    If we queued AutoConnects, we could build a long queue of
    //    useless work (one AutoConnect per Service), which blocks
    //    more timely work.
    ConnectTask();
  } else {
    LOG(INFO) << "Suppressed autoconnect to " << friendly_name();
  }
}

void WiFiService::Connect(Error */*error*/) {
  LOG(INFO) << "In " << __func__ << "():";
  // Defer handling, since dbus-c++ does not permit us to send an
  // outbound request while processing an inbound one.
  dispatcher()->PostTask(
      task_factory_.NewRunnableMethod(&WiFiService::ConnectTask));
}

void WiFiService::Disconnect(Error *error) {
  LOG(INFO) << __func__;
  Service::Disconnect(error);
  // Defer handling, since dbus-c++ does not permit us to send an
  // outbound request while processing an inbound one.
  dispatcher()->PostTask(
      task_factory_.NewRunnableMethod(&WiFiService::DisconnectTask));
}

bool WiFiService::TechnologyIs(const Technology::Identifier type) const {
  return wifi_->TechnologyIs(type);
}

bool WiFiService::IsAutoConnectable() const {
  return Service::IsAutoConnectable() &&
      // Only auto-connect to Services which have visible Endpoints.
      // (Needed because hidden Services may remain registered with
      // Manager even without visible Endpoints.)
      HasEndpoints() &&
      // Do not preempt an existing connection (whether pending, or
      // connected, and whether to this service, or another).
      wifi_->IsIdle();
}

bool WiFiService::IsConnecting() const {
  // WiFi does not move us into the associating state until it gets
  // feedback from wpa_supplicant. So, to answer whether or
  // not we're connecting, we consult with |wifi_|.
  return wifi_->IsConnectingTo(*this);
}

void WiFiService::AddEndpoint(const WiFiEndpointConstRefPtr endpoint) {
  DCHECK(endpoint->ssid() == ssid());
  endpoints_.insert(endpoint);
  UpdateFromEndpoints();
}

void WiFiService::RemoveEndpoint(const WiFiEndpointConstRefPtr endpoint) {
  set<WiFiEndpointConstRefPtr>::iterator i = endpoints_.find(endpoint);
  DCHECK(i != endpoints_.end());
  if (i == endpoints_.end()) {
    LOG(WARNING) << "In " << __func__ << "(): "
                 << "ignorning non-existent endpoint "
                 << endpoint->bssid_string();
    return;
  }
  endpoints_.erase(i);
  if (current_endpoint_ == endpoint) {
    current_endpoint_ = NULL;
  }
  UpdateFromEndpoints();
}

void WiFiService::NotifyCurrentEndpoint(const WiFiEndpoint *endpoint) {
  DCHECK(!endpoint || (endpoints_.find(endpoint) != endpoints_.end()));
  current_endpoint_ = endpoint;
  UpdateFromEndpoints();
}

void WiFiService::NotifyEndpointUpdated(const WiFiEndpoint &endpoint) {
  DCHECK(endpoints_.find(&endpoint) != endpoints_.end());
  UpdateFromEndpoints();
}

string WiFiService::GetStorageIdentifier() const {
  return storage_identifier_;
}

void WiFiService::SetPassphrase(const string &passphrase, Error *error) {
  if (security_ == flimflam::kSecurityWep) {
    ValidateWEPPassphrase(passphrase, error);
  } else if (security_ == flimflam::kSecurityPsk ||
             security_ == flimflam::kSecurityWpa ||
             security_ == flimflam::kSecurityRsn) {
    ValidateWPAPassphrase(passphrase, error);
  } else {
    error->Populate(Error::kNotSupported);
  }

  if (error->IsSuccess()) {
    passphrase_ = passphrase;
  }

  UpdateConnectable();
}

// ClearPassphrase is separate from SetPassphrase, because the default
// value for |passphrase_| would not pass validation.
void WiFiService::ClearPassphrase(Error */*error*/) {
  passphrase_.clear();
  UpdateConnectable();
}

bool WiFiService::IsLoadableFrom(StoreInterface *storage) const {
  return storage->ContainsGroup(GetGenericStorageIdentifier()) ||
      storage->ContainsGroup(GetSpecificStorageIdentifier());
}

bool WiFiService::IsVisible() const {
  // WiFi Services should be displayed only if they are in range (have
  // endpoints that have shown up in a scan) or if the service is actively
  // being connected.
  return HasEndpoints() || IsConnected() || IsConnecting();
}

bool WiFiService::Load(StoreInterface *storage) {
  // First find out which storage identifier is available in priority order
  // of specific, generic.
  string id = GetSpecificStorageIdentifier();
  if (!storage->ContainsGroup(id)) {
    id = GetGenericStorageIdentifier();
    if (!storage->ContainsGroup(id)) {
      LOG(WARNING) << "Service is not available in the persistent store: "
                   << id;
      return false;
    }
  }

  // Set our storage identifier to match the storage name in the Profile.
  storage_identifier_ = id;

  // Load properties common to all Services.
  if (!Service::Load(storage)) {
    return false;
  }

  // Load properties specific to WiFi services.
  storage->GetBool(id, kStorageHiddenSSID, &hidden_ssid_);

  // NB: mode, security and ssid parameters are never read in from
  // Load() as they are provided from the scan.

  string passphrase;
  if (storage->GetCryptedString(id, kStoragePassphrase, &passphrase)) {
    Error error;
    SetPassphrase(passphrase, &error);
    if (!error.IsSuccess()) {
      LOG(ERROR) << "Passphrase could not be set: "
                 << Error::GetName(error.type());
    }
  }

  return true;
}

bool WiFiService::Save(StoreInterface *storage) {
  // Save properties common to all Services.
  if (!Service::Save(storage)) {
    return false;
  }

  // Save properties specific to WiFi services.
  const string id = GetStorageIdentifier();
  storage->SetBool(id, kStorageHiddenSSID, hidden_ssid_);
  storage->SetString(id, kStorageMode, mode_);
  storage->SetCryptedString(id, kStoragePassphrase, passphrase_);
  storage->SetString(id, kStorageSecurity, security_);
  storage->SetString(id, kStorageSSID, hex_ssid_);

  // TODO(quiche): Save Passphrase property. (crosbug.com/23467)
  return true;
}

void WiFiService::Unload() {
  Service::Unload();
  hidden_ssid_ = false;
  passphrase_ = "";
  UpdateConnectable();
  if (security_ == flimflam::kSecurity8021x) {
    // TODO(pstew): 802.1x/RSN networks (as opposed to 802.1x/WPA or
    // 802.1x/WEP) have the ability to cache WPA PMK credentials.
    // Make sure that these are cleared when credentials for networks
    // of this type goes away.
    //
    // When wpa_supplicant gains the ability, do this credential
    // clearing on a per-service basis.  Also do this whenever the credentials
    // for a service changes.  crosbug.com/25670
    wifi_->ClearCachedCredentials();
  }
}

bool WiFiService::IsSecurityMatch(const string &security) const {
  return GetSecurityClass(security) == GetSecurityClass(security_);
}

void WiFiService::InitializeCustomMetrics() const {
  string histogram = metrics()->GetFullMetricName(
                         Metrics::kMetricTimeToJoinMilliseconds,
                         technology());
  metrics()->AddServiceStateTransitionTimer(this,
                                            histogram,
                                            Service::kStateAssociating,
                                            Service::kStateConfiguring);
}

void WiFiService::SendPostReadyStateMetrics() const {
  // TODO(thieule): Send physical mode and security metrics.
  // crosbug.com/24441
  metrics()->SendEnumToUMA(
      metrics()->GetFullMetricName(Metrics::kMetricNetworkChannel,
                                   technology()),
      Metrics::WiFiFrequencyToChannel(frequency_),
      Metrics::kMetricNetworkChannelMax);

  DCHECK(physical_mode_ < Metrics::kWiFiNetworkPhyModeMax);
  metrics()->SendEnumToUMA(
      metrics()->GetFullMetricName(Metrics::kMetricNetworkPhyMode,
                                   technology()),
      static_cast<Metrics::WiFiNetworkPhyMode>(physical_mode_),
      Metrics::kWiFiNetworkPhyModeMax);

  Metrics::WiFiSecurity security_uma =
      Metrics::WiFiSecurityStringToEnum(security_);
  DCHECK(security_uma != Metrics::kWiFiSecurityUnknown);
  metrics()->SendEnumToUMA(
      metrics()->GetFullMetricName(Metrics::kMetricNetworkSecurity,
                                   technology()),
      security_uma,
      Metrics::kMetricNetworkSecurityMax);
}

// private methods
void WiFiService::HelpRegisterWriteOnlyDerivedString(
    const string &name,
    void(WiFiService::*set)(const string &, Error *),
    void(WiFiService::*clear)(Error *),
    const string *default_value) {
  mutable_store()->RegisterDerivedString(
      name,
      StringAccessor(
          new CustomWriteOnlyAccessor<WiFiService, string>(
              this, set, clear, default_value)));
}

void WiFiService::ConnectTask() {
  std::map<string, DBus::Variant> params;
  DBus::MessageIter writer;

  params[wpa_supplicant::kNetworkPropertyMode].writer().
      append_uint32(WiFiEndpoint::ModeStringToUint(mode_));

  if (Is8021x()) {
    // Is EAP key management is not set, set to a default.
    if (GetEAPKeyManagement().empty())
      SetEAPKeyManagement("WPA-EAP");
    Populate8021xProperties(&params);
  } else if (security_ == flimflam::kSecurityPsk) {
    const string psk_proto = StringPrintf("%s %s",
                                          wpa_supplicant::kSecurityModeWPA,
                                          wpa_supplicant::kSecurityModeRSN);
    params[wpa_supplicant::kPropertySecurityProtocol].writer().
        append_string(psk_proto.c_str());
    params[wpa_supplicant::kPropertyPreSharedKey].writer().
        append_string(passphrase_.c_str());
  } else if (security_ == flimflam::kSecurityRsn) {
    params[wpa_supplicant::kPropertySecurityProtocol].writer().
        append_string(wpa_supplicant::kSecurityModeRSN);
    params[wpa_supplicant::kPropertyPreSharedKey].writer().
        append_string(passphrase_.c_str());
  } else if (security_ == flimflam::kSecurityWpa) {
    params[wpa_supplicant::kPropertySecurityProtocol].writer().
        append_string(wpa_supplicant::kSecurityModeWPA);
    params[wpa_supplicant::kPropertyPreSharedKey].writer().
        append_string(passphrase_.c_str());
  } else if (security_ == flimflam::kSecurityWep) {
    params[wpa_supplicant::kPropertyAuthAlg].writer().
        append_string(wpa_supplicant::kSecurityAuthAlg);
    Error error;
    int key_index;
    std::vector<uint8> password_bytes;
    ParseWEPPassphrase(passphrase_, &key_index, &password_bytes, &error);
    writer = params[wpa_supplicant::kPropertyWEPKey +
                    base::IntToString(key_index)].writer();
    writer << password_bytes;
    params[wpa_supplicant::kPropertyWEPTxKeyIndex].writer().
        append_uint32(key_index);
  } else if (security_ == flimflam::kSecurityNone) {
    // Nothing special to do here.
  } else {
    LOG(ERROR) << "Can't connect. Unsupported security method " << security_;
  }

  params[wpa_supplicant::kNetworkPropertyEapKeyManagement].writer().
      append_string(key_management().c_str());

  // See note in dbus_adaptor.cc on why we need to use a local.
  writer = params[wpa_supplicant::kNetworkPropertySSID].writer();
  writer << ssid_;

  wifi_->ConnectTo(this, params);
}

void WiFiService::DisconnectTask() {
  wifi_->DisconnectFrom(this);
}

string WiFiService::GetDeviceRpcId(Error */*error*/) {
  return wifi_->GetRpcIdentifier();
}

void WiFiService::UpdateConnectable() {
  bool is_connectable = false;
  if (security_ == flimflam::kSecurityNone) {
    DCHECK(passphrase_.empty());
    need_passphrase_ = false;
    is_connectable = true;
  } else if (Is8021x()) {
    is_connectable = Is8021xConnectable();
  } else if (security_ == flimflam::kSecurityWep ||
      security_ == flimflam::kSecurityWpa ||
      security_ == flimflam::kSecurityPsk ||
      security_ == flimflam::kSecurityRsn) {
    need_passphrase_ = passphrase_.empty();
    is_connectable = !need_passphrase_;
  }
  set_connectable(is_connectable);
}

void WiFiService::UpdateFromEndpoints() {
  const WiFiEndpoint *representative_endpoint = NULL;

  if (current_endpoint_) {
    // TODO: Copy BSSID here (crosbug.com/22377).
    representative_endpoint = current_endpoint_;
  } else  {
    int16 best_signal = std::numeric_limits<int16>::min();
    for (set<WiFiEndpointConstRefPtr>::iterator i = endpoints_.begin();
         i != endpoints_.end(); ++i) {
      if ((*i)->signal_strength() >= best_signal) {
        best_signal = (*i)->signal_strength();
        representative_endpoint = *i;
      }
    }
  }

  uint16 frequency;
  int16 signal;
  if (!representative_endpoint) {
    frequency = 0;
    signal = std::numeric_limits<int16>::min();
  } else {
    frequency = representative_endpoint->frequency();
    signal = representative_endpoint->signal_strength();
  }

  if (frequency_ != frequency) {
    frequency_ = frequency;
    adaptor()->EmitUint16Changed(flimflam::kWifiFrequency, frequency_);
  }
  SetStrength(SignalToStrength(signal));
}

// static
void WiFiService::ValidateWEPPassphrase(const std::string &passphrase,
                                        Error *error) {
  ParseWEPPassphrase(passphrase, NULL, NULL, error);
}

// static
void WiFiService::ValidateWPAPassphrase(const std::string &passphrase,
                                        Error *error) {
  unsigned int length = passphrase.length();
  vector<uint8> passphrase_bytes;

  if (base::HexStringToBytes(passphrase, &passphrase_bytes)) {
    if (length != IEEE_80211::kWPAHexLen &&
        (length < IEEE_80211::kWPAAsciiMinLen ||
         length > IEEE_80211::kWPAAsciiMaxLen)) {
      error->Populate(Error::kInvalidPassphrase);
    }
  } else {
    if (length < IEEE_80211::kWPAAsciiMinLen ||
        length > IEEE_80211::kWPAAsciiMaxLen) {
      error->Populate(Error::kInvalidPassphrase);
    }
  }
}

// static
void WiFiService::ParseWEPPassphrase(const string &passphrase,
                                     int *key_index,
                                     std::vector<uint8> *password_bytes,
                                     Error *error) {
  unsigned int length = passphrase.length();
  int key_index_local;
  std::string password_text;
  bool is_hex = false;

  switch (length) {
    case IEEE_80211::kWEP40AsciiLen:
    case IEEE_80211::kWEP104AsciiLen:
      key_index_local = 0;
      password_text = passphrase;
      break;
    case IEEE_80211::kWEP40AsciiLen + 2:
    case IEEE_80211::kWEP104AsciiLen + 2:
      if (CheckWEPKeyIndex(passphrase, error)) {
        base::StringToInt(passphrase.substr(0,1), &key_index_local);
        password_text = passphrase.substr(2);
      }
      break;
    case IEEE_80211::kWEP40HexLen:
    case IEEE_80211::kWEP104HexLen:
      if (CheckWEPIsHex(passphrase, error)) {
        key_index_local = 0;
        password_text = passphrase;
        is_hex = true;
      }
      break;
    case IEEE_80211::kWEP40HexLen + 2:
    case IEEE_80211::kWEP104HexLen + 2:
      if(CheckWEPKeyIndex(passphrase, error) &&
         CheckWEPIsHex(passphrase.substr(2), error)) {
        base::StringToInt(passphrase.substr(0,1), &key_index_local);
        password_text = passphrase.substr(2);
        is_hex = true;
      } else if (CheckWEPPrefix(passphrase, error) &&
                 CheckWEPIsHex(passphrase.substr(2), error)) {
        key_index_local = 0;
        password_text = passphrase.substr(2);
        is_hex = true;
      }
      break;
    case IEEE_80211::kWEP40HexLen + 4:
    case IEEE_80211::kWEP104HexLen + 4:
      if (CheckWEPKeyIndex(passphrase, error) &&
          CheckWEPPrefix(passphrase.substr(2), error) &&
          CheckWEPIsHex(passphrase.substr(4), error)) {
        base::StringToInt(passphrase.substr(0,1), &key_index_local);
        password_text = passphrase.substr(4);
        is_hex = true;
      }
      break;
    default:
      error->Populate(Error::kInvalidPassphrase);
      break;
  }

  if (error->IsSuccess()) {
    if (key_index)
      *key_index = key_index_local;
    if (password_bytes) {
      if (is_hex)
        base::HexStringToBytes(password_text, password_bytes);
      else
        password_bytes->insert(password_bytes->end(),
                               password_text.begin(),
                               password_text.end());
    }
  }
}

// static
bool WiFiService::CheckWEPIsHex(const string &passphrase, Error *error) {
  vector<uint8> passphrase_bytes;
  if (base::HexStringToBytes(passphrase, &passphrase_bytes)) {
    return true;
  } else {
    error->Populate(Error::kInvalidPassphrase);
    return false;
  }
}

// static
bool WiFiService::CheckWEPKeyIndex(const string &passphrase, Error *error) {
  if (StartsWithASCII(passphrase, "0:", false) ||
      StartsWithASCII(passphrase, "1:", false) ||
      StartsWithASCII(passphrase, "2:", false) ||
      StartsWithASCII(passphrase, "3:", false)) {
    return true;
  } else {
    error->Populate(Error::kInvalidPassphrase);
    return false;
  }
}

// static
bool WiFiService::CheckWEPPrefix(const string &passphrase, Error *error) {
  if (StartsWithASCII(passphrase, "0x", false)) {
    return true;
  } else {
    error->Populate(Error::kInvalidPassphrase);
    return false;
  }
}

// static
string WiFiService::GetSecurityClass(const string &security) {
  if (security == flimflam::kSecurityRsn ||
      security == flimflam::kSecurityWpa) {
    return flimflam::kSecurityPsk;
  } else {
    return security;
  }
}

// static
bool WiFiService::ParseStorageIdentifier(const string &storage_name,
                                         string *address,
                                         string *mode,
                                         string *security) {
  vector<string> wifi_parts;
  base::SplitString(storage_name, '_', &wifi_parts);
  if ((wifi_parts.size() != 5 && wifi_parts.size() != 6) ||
      wifi_parts[0] != flimflam::kTypeWifi) {
    return false;
  }
  *address = wifi_parts[1];
  *mode = wifi_parts[3];
  if (wifi_parts.size() == 5) {
    *security = wifi_parts[4];
  } else {
    // Account for security type "802_1x" which got split up above.
    *security = wifi_parts[4] + "_" + wifi_parts[5];
  }
  return true;
}

// static
uint8 WiFiService::SignalToStrength(int16 signal_dbm) {
  int16 strength;
  if (signal_dbm > 0) {
    if (!logged_signal_warning) {
      LOG(WARNING) << "Signal strength is suspiciously high. "
                   << "Assuming value " << signal_dbm << " is not in dBm.";
      logged_signal_warning = true;
    }
    strength = signal_dbm;
  } else {
    strength = 120 + signal_dbm;  // Call -20dBm "perfect".
  }

  if (strength > kStrengthMax) {
    strength = kStrengthMax;
  } else if (strength < kStrengthMin) {
    strength = kStrengthMin;
  }
  return strength;
}

string WiFiService::GetGenericStorageIdentifier() const {
  return GetStorageIdentifierForSecurity(GetSecurityClass(security_));
}

string WiFiService::GetSpecificStorageIdentifier() const {
  return GetStorageIdentifierForSecurity(security_);
}

string WiFiService::GetStorageIdentifierForSecurity(
    const string &security) const {
   return StringToLowerASCII(base::StringPrintf("%s_%s_%s_%s_%s",
                                               flimflam::kTypeWifi,
                                               wifi_->address().c_str(),
                                               hex_ssid_.c_str(),
                                               mode_.c_str(),
                                               security.c_str()));
}

void WiFiService::set_eap(const EapCredentials &eap) {
  Service::set_eap(eap);
  UpdateConnectable();
}

bool WiFiService::Is8021x() const {
  if (security_ == flimflam::kSecurity8021x)
    return true;

  // Dynamic WEP + 802.1x.
  if (security_ == flimflam::kSecurityWep &&
      GetEAPKeyManagement() == "IEEE8021X")
    return true;
  return false;
}

void WiFiService::Populate8021xProperties(
    std::map<string, DBus::Variant> *params) {
  typedef std::pair<const char *, const char *> KeyVal;
  KeyVal propertyvals[] = {
    KeyVal(wpa_supplicant::kNetworkPropertyEapIdentity, eap().identity.c_str()),
    KeyVal(wpa_supplicant::kNetworkPropertyEapEap, eap().eap.c_str()),
    KeyVal(wpa_supplicant::kNetworkPropertyEapInnerEap,
           eap().inner_eap.c_str()),
    KeyVal(wpa_supplicant::kNetworkPropertyEapAnonymousIdentity,
           eap().anonymous_identity.c_str()),
    KeyVal(wpa_supplicant::kNetworkPropertyEapClientCert,
           eap().client_cert.c_str()),
    KeyVal(wpa_supplicant::kNetworkPropertyEapPrivateKey,
           eap().private_key.c_str()),
    KeyVal(wpa_supplicant::kNetworkPropertyEapPrivateKeyPassword,
           eap().private_key_password.c_str()),
    KeyVal(wpa_supplicant::kNetworkPropertyEapCaCert, eap().ca_cert.c_str()),
    KeyVal(wpa_supplicant::kNetworkPropertyEapCaPassword,
           eap().password.c_str()),
    KeyVal(wpa_supplicant::kNetworkPropertyEapCertId, eap().cert_id.c_str()),
    KeyVal(wpa_supplicant::kNetworkPropertyEapKeyId, eap().key_id.c_str()),
    KeyVal(wpa_supplicant::kNetworkPropertyEapCaCertId,
           eap().ca_cert_id.c_str()),
    KeyVal(wpa_supplicant::kNetworkPropertyEapPin, eap().pin.c_str()),
    // TODO(gauravsh): Support getting CA certificates out of the NSS certdb.
    //                 crosbug.com/25663
    KeyVal(wpa_supplicant::kNetworkPropertyCaPath, wpa_supplicant::kCaPath)
  };

  DBus::MessageIter writer;
  for (size_t i = 0; i < arraysize(propertyvals); ++i) {
    if (strlen(propertyvals[i].second) > 0) {
      (*params)[propertyvals[i].first].writer().
          append_string(propertyvals[i].second);
    }
  }
}

}  // namespace shill
