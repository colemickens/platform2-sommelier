// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/wifi_service.h"

#include <string>
#include <utility>

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
#include "shill/logging.h"
#include "shill/manager.h"
#include "shill/metrics.h"
#include "shill/nss.h"
#include "shill/property_accessor.h"
#include "shill/store_interface.h"
#include "shill/wifi.h"
#include "shill/wifi_endpoint.h"
#include "shill/wifi_provider.h"
#include "shill/wpa_supplicant.h"

using std::set;
using std::string;
using std::vector;

namespace shill {

const char WiFiService::kAutoConnNoEndpoint[] = "no endpoints";
const char WiFiService::kAnyDeviceAddress[] = "any";

const char WiFiService::kStorageHiddenSSID[] = "WiFi.HiddenSSID";
const char WiFiService::kStorageMode[] = "WiFi.Mode";
const char WiFiService::kStoragePassphrase[] = "Passphrase";
const char WiFiService::kStorageSecurity[] = "WiFi.Security";
const char WiFiService::kStorageSecurityClass[] = "WiFi.SecurityClass";
const char WiFiService::kStorageSSID[] = "SSID";
bool WiFiService::logged_signal_warning = false;

WiFiService::WiFiService(ControlInterface *control_interface,
                         EventDispatcher *dispatcher,
                         Metrics *metrics,
                         Manager *manager,
                         WiFiProvider *provider,
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
      raw_signal_strength_(0),
      ssid_(ssid),
      ieee80211w_required_(false),
      nss_(NSS::GetInstance()),
      provider_(provider) {
  PropertyStore *store = this->mutable_store();
  store->RegisterConstString(flimflam::kModeProperty, &mode_);
  HelpRegisterWriteOnlyDerivedString(flimflam::kPassphraseProperty,
                                     &WiFiService::SetPassphrase,
                                     &WiFiService::ClearPassphrase,
                                     NULL);
  store->RegisterBool(flimflam::kPassphraseRequiredProperty, &need_passphrase_);
  store->RegisterConstString(flimflam::kSecurityProperty, &security_);

  store->RegisterConstString(flimflam::kWifiAuthMode, &auth_mode_);
  store->RegisterBool(flimflam::kWifiHiddenSsid, &hidden_ssid_);
  store->RegisterConstUint16(flimflam::kWifiFrequency, &frequency_);
  store->RegisterConstUint16(flimflam::kWifiPhyMode, &physical_mode_);
  store->RegisterConstString(flimflam::kWifiBSsid, &bssid_);
  store->RegisterConstStringmap(kWifiVendorInformationProperty,
                                &vendor_information_);
  store->RegisterConstBool(kWifiProtectedManagementFrameRequiredProperty,
                           &ieee80211w_required_);

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
  storage_identifier_ = GetDefaultStorageIdentifier();
  UpdateConnectable();

  IgnoreParameterForConfigure(flimflam::kModeProperty);
  IgnoreParameterForConfigure(flimflam::kSSIDProperty);
  IgnoreParameterForConfigure(flimflam::kSecurityProperty);

  // Log the |unique_name| to |friendly_name| mapping for debugging purposes at
  // non-default log level.
  SLOG(WiFi, 1) << "Constructed WiFi service " << unique_name()
                << " name: " << friendly_name();
}

WiFiService::~WiFiService() {}

bool WiFiService::IsAutoConnectable(const char **reason) const {
  if (!Service::IsAutoConnectable(reason)) {
    return false;
  }

  // Only auto-connect to Services which have visible Endpoints.
  // (Needed because hidden Services may remain registered with
  // Manager even without visible Endpoints.)
  if (!HasEndpoints()) {
    *reason = kAutoConnNoEndpoint;
    return false;
  }

  CHECK(wifi_) << "We have endpoints but no WiFi device is selected?";

  // Do not preempt an existing connection (whether pending, or
  // connected, and whether to this service, or another).
  if (!wifi_->IsIdle()) {
    *reason = kAutoConnBusy;
    return false;
  }

  return true;
}

void WiFiService::AddEndpoint(const WiFiEndpointConstRefPtr &endpoint) {
  DCHECK(endpoint->ssid() == ssid());
  endpoints_.insert(endpoint);
  UpdateFromEndpoints();
}

void WiFiService::RemoveEndpoint(const WiFiEndpointConstRefPtr &endpoint) {
  set<WiFiEndpointConstRefPtr>::iterator i = endpoints_.find(endpoint);
  DCHECK(i != endpoints_.end());
  if (i == endpoints_.end()) {
    LOG(WARNING) << "In " << __func__ << "(): "
                 << "ignoring non-existent endpoint "
                 << endpoint->bssid_string();
    return;
  }
  endpoints_.erase(i);
  if (current_endpoint_ == endpoint) {
    current_endpoint_ = NULL;
  }
  UpdateFromEndpoints();
}

void WiFiService::NotifyCurrentEndpoint(
    const WiFiEndpointConstRefPtr &endpoint) {
  DCHECK(!endpoint || (endpoints_.find(endpoint) != endpoints_.end()));
  current_endpoint_ = endpoint;
  UpdateFromEndpoints();
}

void WiFiService::NotifyEndpointUpdated(
    const WiFiEndpointConstRefPtr &endpoint) {
  DCHECK(endpoints_.find(endpoint) != endpoints_.end());
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

  if (!error->IsSuccess() || passphrase == passphrase_) {
    return;
  }

  passphrase_ = passphrase;
  ClearCachedCredentials();
  UpdateConnectable();
}

// ClearPassphrase is separate from SetPassphrase, because the default
// value for |passphrase_| would not pass validation.
void WiFiService::ClearPassphrase(Error */*error*/) {
  passphrase_.clear();
  ClearCachedCredentials();
  UpdateConnectable();
}

bool WiFiService::IsLoadableFrom(StoreInterface *storage) const {
  return !storage->GetGroupsWithProperties(GetStorageProperties()).empty();
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
  set<string> groups = storage->GetGroupsWithProperties(GetStorageProperties());
  if (groups.empty()) {
    LOG(WARNING) << "Configuration for service "
                 << unique_name()
                 << " is not available in the persistent store";
    return false;
  }
  if (groups.size() > 0) {
    LOG(WARNING) << "More than one configuration for service "
                 << unique_name()
                 << " is available; choosing the first.";
  }
  string id = *groups.begin();

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
  storage->SetString(id, kStorageSecurityClass, GetSecurityClass(security_));
  storage->SetString(id, kStorageSSID, hex_ssid_);

  return true;
}

bool WiFiService::Unload() {
  Service::Unload();
  hidden_ssid_ = false;
  Error unused_error;
  ClearPassphrase(&unused_error);
  return provider_->OnServiceUnloaded(this);
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

void WiFiService::SendPostReadyStateMetrics(
    int64 time_resume_to_ready_milliseconds) const {
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

  // We invert the sign of the signal strength value, since UMA histograms
  // cannot represent negative numbers (it stores them but cannot display
  // them), and dBm values of interest start at 0 and go negative from there.
  metrics()->SendToUMA(
      metrics()->GetFullMetricName(Metrics::kMetricNetworkSignalStrength,
                                   technology()),
      -raw_signal_strength_,
      Metrics::kMetricNetworkSignalStrengthMin,
      Metrics::kMetricNetworkSignalStrengthMax,
      Metrics::kMetricNetworkSignalStrengthNumBuckets);

  if (time_resume_to_ready_milliseconds > 0) {
    metrics()->SendToUMA(
        metrics()->GetFullMetricName(
            Metrics::kMetricTimeResumeToReadyMilliseconds, technology()),
        time_resume_to_ready_milliseconds,
        Metrics::kTimerHistogramMillisecondsMin,
        Metrics::kTimerHistogramMillisecondsMax,
        Metrics::kTimerHistogramNumBuckets);
  }
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

void WiFiService::Connect(Error *error) {
  LOG(INFO) << "Connect to service " << unique_name();
  std::map<string, DBus::Variant> params;
  DBus::MessageIter writer;

  if (!connectable()) {
    LOG(ERROR) << "Can't connect. Service " << unique_name()
               << " is not connectable.";
    Error::PopulateAndLog(error,
                          Error::kOperationFailed,
                          Error::GetDefaultMessage(Error::kOperationFailed));
    return;
  }
  if (IsConnecting() || IsConnected()) {
    LOG(WARNING) << "Can't connect.  Service " << unique_name()
                 << " is already connecting or connected.";
    Error::PopulateAndLog(error,
                          Error::kAlreadyConnected,
                          Error::GetDefaultMessage(Error::kAlreadyConnected));
    return;
  }

  WiFiRefPtr wifi = wifi_;
  if (!wifi) {
    // If this is a hidden service before it has been found in a scan, we
    // may need to late-bind to any available WiFi Device.  We don't actually
    // set |wifi_| in this case snce we do not yet see any endpoints.  This
    // will mean this service is not disconnectable until an endpoint is
    // found.
    wifi = ChooseDevice();
    if (!wifi) {
      LOG(ERROR) << "Can't connect. Service " << unique_name()
                 << " cannot find a WiFi device.";
      Error::PopulateAndLog(error,
                            Error::kOperationFailed,
                            Error::GetDefaultMessage(Error::kOperationFailed));
      return;
    }
  }

  if (wifi->IsCurrentService(this)) {
    LOG(WARNING) << "Can't connect.  Service " << unique_name()
                 << " is the current service (but, in " << GetStateString()
                 << " state, not connected).";
    Error::PopulateAndLog(error,
                          Error::kInProgress,
                          Error::GetDefaultMessage(Error::kInProgress));
    return;
  }

  params[wpa_supplicant::kNetworkPropertyMode].writer().
      append_uint32(WiFiEndpoint::ModeStringToUint(mode_));

  if (mode_ == flimflam::kModeAdhoc && frequency_ != 0) {
    // Frequency is required in order to successfully conntect to an IBSS
    // with wpa_supplicant.  If we have one from our endpoint, insert it
    // here.
    params[wpa_supplicant::kNetworkPropertyFrequency].writer().
        append_int32(frequency_);
  }

  if (Is8021x()) {
    // Is EAP key management is not set, set to a default.
    if (GetEAPKeyManagement().empty())
      SetEAPKeyManagement("WPA-EAP");
    Populate8021xProperties(&params);
    ClearEAPCertification();
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

  if (ieee80211w_required_) {
    // TODO(pstew): We should also enable IEEE 802.11w if the user
    // explicitly enables support for this through a service / device
    // property.  crosbug.com/37800
    params[wpa_supplicant::kNetworkPropertyIeee80211w].writer().
        append_uint32(wpa_supplicant::kNetworkIeee80211wEnabled);
  }

  // See note in dbus_adaptor.cc on why we need to use a local.
  writer = params[wpa_supplicant::kNetworkPropertySSID].writer();
  writer << ssid_;

  wifi->ConnectTo(this, params);
}

void WiFiService::Disconnect(Error *error) {
  LOG(INFO) << __func__;
  Service::Disconnect(error);
  if (!wifi_) {
    // If we are connecting to a hidden service, but have not yet found
    // any endpoints, we could end up with a disconnect request without
    // a wifi_ reference.  This is not a fatal error.
    LOG_IF(ERROR, IsConnecting())
         << "WiFi endpoints do not (yet) exist.  Cannot disconnect service "
         << unique_name();
    LOG_IF(FATAL, IsConnected())
         << "WiFi device does not exist.  Cannot disconnect service "
         << unique_name();
    error->Populate(Error::kOperationFailed);
    return;
  }
  wifi_->DisconnectFrom(this);
}

string WiFiService::GetDeviceRpcId(Error *error) {
  if (!wifi_) {
    error->Populate(Error::kNotFound, "Not associated with a device");
    return "/";
  }
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

  WiFiRefPtr wifi;
  if (representative_endpoint) {
    wifi = representative_endpoint->device();
  } else if (IsConnected() || IsConnecting()) {
    LOG(WARNING) << "Service " << unique_name()
                 << " will disconnect due to no remaining endpoints.";
  }

  SetWiFi(wifi);

  for (set<WiFiEndpointConstRefPtr>::iterator i = endpoints_.begin();
       i != endpoints_.end(); ++i) {
    if ((*i)->ieee80211w_required()) {
      // Never reset ieee80211w_required_ to false, so we track whether we have
      // ever seen an AP that requires 802.11w.
      ieee80211w_required_ = true;
    }
  }

  uint16 frequency = 0;
  int16 signal = std::numeric_limits<int16>::min();
  string bssid;
  Stringmap vendor_information;
  // Represent "unknown raw signal strength" as 0.
  raw_signal_strength_ = 0;
  if (representative_endpoint) {
    frequency = representative_endpoint->frequency();
    signal = representative_endpoint->signal_strength();
    raw_signal_strength_ = signal;
    bssid = representative_endpoint->bssid_string();
    vendor_information = representative_endpoint->GetVendorInformation();
  }

  if (frequency_ != frequency) {
    frequency_ = frequency;
    adaptor()->EmitUint16Changed(flimflam::kWifiFrequency, frequency_);
  }
  if (bssid_ != bssid) {
    bssid_ = bssid;
    adaptor()->EmitStringChanged(flimflam::kWifiBSsid, bssid_);
  }
  if (vendor_information_ != vendor_information) {
    vendor_information_ = vendor_information;
    adaptor()->EmitStringmapChanged(kWifiVendorInformationProperty,
                                    vendor_information_);
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
bool WiFiService::FixupServiceEntries(StoreInterface *storage) {
  bool fixed_entry = false;
  set<string> groups = storage->GetGroups();
  for (set<string>::const_iterator it = groups.begin(); it != groups.end();
       ++it) {
    const string &id = *it;
    string device_address, network_mode, security;
    if (!ParseStorageIdentifier(id, &device_address,
                                &network_mode, &security)) {
      continue;
    }
    if (!storage->GetString(id, kStorageType, NULL)) {
      storage->SetString(id, kStorageType, flimflam::kTypeWifi);
      fixed_entry = true;
    }
    if (!storage->GetString(id, kStorageMode, NULL)) {
      storage->SetString(id, kStorageMode, network_mode);
      fixed_entry = true;
    }
    if (!storage->GetString(id, kStorageSecurity, NULL)) {
      storage->SetString(id, kStorageSecurity, security);
      fixed_entry = true;
    }
    if (!storage->GetString(id, kStorageSecurityClass, NULL)) {
      storage->SetString(id, kStorageSecurityClass, GetSecurityClass(security));
      fixed_entry = true;
    }
  }
  return fixed_entry;
}

// static
bool WiFiService::IsValidSecurityMethod(const string &method) {
  return method == flimflam::kSecurityNone ||
      method == flimflam::kSecurityWep ||
      method == flimflam::kSecurityPsk ||
      method == flimflam::kSecurityWpa ||
      method == flimflam::kSecurityRsn ||
      method == flimflam::kSecurity8021x;
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

KeyValueStore WiFiService::GetStorageProperties() const {
  KeyValueStore args;
  args.SetString(kStorageType, flimflam::kTypeWifi);
  args.SetString(kStorageSSID, hex_ssid_);
  args.SetString(kStorageMode, mode_);
  args.SetString(kStorageSecurityClass, GetSecurityClass(security_));
  return args;
}

string WiFiService::GetDefaultStorageIdentifier() const {
  string security = GetSecurityClass(security_);
  return StringToLowerASCII(base::StringPrintf("%s_%s_%s_%s_%s",
                                               flimflam::kTypeWifi,
                                               kAnyDeviceAddress,
                                               hex_ssid_.c_str(),
                                               mode_.c_str(),
                                               security.c_str()));
}

void WiFiService::ClearCachedCredentials() {
  if (wifi_) {
    wifi_->ClearCachedCredentials(this);
  }
}

void WiFiService::set_eap(const EapCredentials &new_eap) {
  EapCredentials modified_eap = new_eap;

  // An empty key_management field is invalid.  Prevent it, if possible.
  if (modified_eap.key_management.empty()) {
    modified_eap.key_management = eap().key_management;
  }
  Service::set_eap(modified_eap);
  ClearCachedCredentials();
  UpdateConnectable();
}

void WiFiService::OnProfileConfigured() {
  if (profile() || !hidden_ssid()) {
    return;
  }
  // This situation occurs when a hidden WiFi service created via GetService
  // has been persisted to a profile in Manager::ConfigureService().  Now
  // that configuration is saved, we must join the service with its profile,
  // which will make this SSID eligible for directed probes during scans.
  manager()->RegisterService(this);
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
  string ca_cert = eap().ca_cert;
  if (!eap().ca_cert_nss.empty()) {
    vector<char> id(ssid_.begin(), ssid_.end());
    FilePath certfile = nss_->GetDERCertfile(eap().ca_cert_nss, id);
    if (certfile.empty()) {
      LOG(ERROR) << "Unable to extract certificate: " << eap().ca_cert_nss;
    } else {
      ca_cert = certfile.value();
    }
  }


  typedef std::pair<const char *, const char *> KeyVal;
  KeyVal init_propertyvals[] = {
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
    KeyVal(wpa_supplicant::kNetworkPropertyEapCaCert, ca_cert.c_str()),
    KeyVal(wpa_supplicant::kNetworkPropertyEapCaPassword,
           eap().password.c_str()),
    KeyVal(wpa_supplicant::kNetworkPropertyEapCertId, eap().cert_id.c_str()),
    KeyVal(wpa_supplicant::kNetworkPropertyEapKeyId, eap().key_id.c_str()),
    KeyVal(wpa_supplicant::kNetworkPropertyEapCaCertId,
           eap().ca_cert_id.c_str()),
    KeyVal(wpa_supplicant::kNetworkPropertyEapSubjectMatch,
           eap().subject_match.c_str())
  };

  vector<KeyVal> propertyvals(init_propertyvals,
                              init_propertyvals + arraysize(init_propertyvals));
  if (eap().use_system_cas) {
    propertyvals.push_back(KeyVal(
        wpa_supplicant::kNetworkPropertyCaPath, wpa_supplicant::kCaPath));
  } else if (ca_cert.empty()) {
      LOG(WARNING) << __func__
                   << ": No certificate authorities are configured."
                   << " Server certificates will be accepted"
                   << " unconditionally.";
  }

  if (!eap().cert_id.empty() || !eap().key_id.empty() ||
      !eap().ca_cert_id.empty()) {
    propertyvals.push_back(KeyVal(
        wpa_supplicant::kNetworkPropertyEapPin, eap().pin.c_str()));
    propertyvals.push_back(KeyVal(
        wpa_supplicant::kNetworkPropertyEngineId,
        wpa_supplicant::kEnginePKCS11));
    // We can't use the propertyvals vector for this since this argument
    // is a uint32, not a string.
    (*params)[wpa_supplicant::kNetworkPropertyEngine].writer().
        append_uint32(wpa_supplicant::kDefaultEngine);
  }

  vector<KeyVal>::iterator it;
  for (it = propertyvals.begin(); it != propertyvals.end(); ++it) {
    if (strlen((*it).second) > 0) {
      (*params)[(*it).first].writer().append_string((*it).second);
    }
  }
}

WiFiRefPtr WiFiService::ChooseDevice() {
  // TODO(pstew): Style frowns on dynamic_cast.  crosbug.com/38237
  DeviceRefPtr device =
      manager()->GetEnabledDeviceWithTechnology(Technology::kWifi);
  return dynamic_cast<WiFi *>(device.get());
}

void WiFiService::ResetWiFi() {
  SetWiFi(NULL);
}

void WiFiService::SetWiFi(const WiFiRefPtr &wifi) {
  if (wifi_ == wifi) {
    return;
  }
  ClearCachedCredentials();
  if (wifi_) {
    wifi_->DisassociateFromService(this);
  }
  wifi_ = wifi;
}

}  // namespace shill
