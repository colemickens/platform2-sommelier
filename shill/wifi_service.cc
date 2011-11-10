// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/wifi_service.h"

#include <string>

#include <base/logging.h>
#include <base/stringprintf.h>
#include <base/string_number_conversions.h>
#include <base/string_util.h>
#include <chromeos/dbus/service_constants.h>
#include <dbus/dbus.h>
#include <glib.h>

#include "shill/control_interface.h"
#include "shill/device.h"
#include "shill/error.h"
#include "shill/event_dispatcher.h"
#include "shill/ieee80211.h"
#include "shill/store_interface.h"
#include "shill/wifi.h"
#include "shill/wifi_endpoint.h"
#include "shill/wpa_supplicant.h"

using std::string;
using std::vector;

namespace shill {

const char WiFiService::kStorageHiddenSSID[] = "WiFi.HiddenSSID";

WiFiService::WiFiService(ControlInterface *control_interface,
                         EventDispatcher *dispatcher,
                         Manager *manager,
                         const WiFiRefPtr &device,
                         const std::vector<uint8_t> &ssid,
                         const std::string &mode,
                         const std::string &security,
                         bool hidden_ssid)
    : Service(control_interface, dispatcher, manager, flimflam::kTypeWifi),
      need_passphrase_(false),
      security_(security),
      mode_(mode),
      hidden_ssid_(hidden_ssid),
      task_factory_(this),
      wifi_(device),
      ssid_(ssid) {
  PropertyStore *store = this->mutable_store();
  store->RegisterConstString(flimflam::kModeProperty, &mode_);
  store->RegisterString(flimflam::kPassphraseProperty, &passphrase_);
  store->RegisterBool(flimflam::kPassphraseRequiredProperty, &need_passphrase_);
  store->RegisterConstString(flimflam::kSecurityProperty, &security_);
  store->RegisterConstUint8(flimflam::kSignalStrengthProperty, &strength_);

  store->RegisterConstString(flimflam::kWifiAuthMode, &auth_mode_);
  store->RegisterConstBool(flimflam::kWifiHiddenSsid, &hidden_ssid_);
  store->RegisterConstUint16(flimflam::kWifiFrequency, &frequency_);
  store->RegisterConstUint16(flimflam::kWifiPhyMode, &physical_mode_);

  hex_ssid_ = base::HexEncode(ssid_.data(), ssid_.size());
  string ssid_string(
      reinterpret_cast<const char *>(ssid_.data()), ssid_.size());
  if (SanitizeSSID(&ssid_string)) {
    // WifiHexSsid property should only be present if Name property
    // has been munged.
    store->RegisterConstString(flimflam::kWifiHexSsid, &hex_ssid_);
  }
  set_friendly_name(ssid_string);

  // TODO(quiche): determine if it is okay to set EAP.KeyManagement for
  // a service that is not 802.1x.
  if (security_ == flimflam::kSecurity8021x) {
    NOTIMPLEMENTED();
    // XXX needs_passpharse_ = false ?
  } else if (security_ == flimflam::kSecurityPsk) {
    SetEAPKeyManagement("WPA-PSK");
    need_passphrase_ = true;
  } else if (security_ == flimflam::kSecurityRsn) {
    SetEAPKeyManagement("WPA-PSK");
    need_passphrase_ = true;
  } else if (security_ == flimflam::kSecurityWpa) {
    SetEAPKeyManagement("WPA-PSK");
    need_passphrase_ = true;
  } else if (security_ == flimflam::kSecurityWep) {
    SetEAPKeyManagement("NONE");
    need_passphrase_ = true;
  } else if (security_ == flimflam::kSecurityNone) {
    SetEAPKeyManagement("NONE");
    need_passphrase_ = false;
  } else {
    LOG(ERROR) << "unsupported security method " << security_;
  }

  // Until we know better (at Profile load time), use the generic name.
  storage_identifier_ = GetGenericStorageIdentifier();
}

WiFiService::~WiFiService() {
  LOG(INFO) << __func__;
}

void WiFiService::Connect(Error */*error*/) {
  LOG(INFO) << __func__;

  // NB(quiche) defer handling, since dbus-c++ does not permit us to
  // send an outbound request while processing an inbound one.
  dispatcher()->PostTask(
      task_factory_.NewRunnableMethod(&WiFiService::ConnectTask));
}

void WiFiService::Disconnect() {
  // TODO(quiche) RemoveNetwork from supplicant
  // XXX remove from favorite networks list?
}

bool WiFiService::TechnologyIs(const Technology::Identifier type) const {
  return wifi_->TechnologyIs(type);
}

string WiFiService::GetStorageIdentifier() const {
  return storage_identifier_;
}
const string &WiFiService::mode() const {
  return mode_;
}

const string &WiFiService::key_management() const {
  return GetEAPKeyManagement();
}

const std::vector<uint8_t> &WiFiService::ssid() const {
  return ssid_;
}

void WiFiService::SetPassphrase(const string &passphrase, Error *error) {
  if (security_ == flimflam::kSecurityWep) {
    passphrase_ = ParseWEPPassphrase(passphrase, error);
  } else if (security_ == flimflam::kSecurityPsk ||
             security_ == flimflam::kSecurityWpa ||
             security_ == flimflam::kSecurityRsn) {
    passphrase_ = ParseWPAPassphrase(passphrase, error);
  }
}

bool WiFiService::IsLoadableFrom(StoreInterface *storage) const {
  return storage->ContainsGroup(GetGenericStorageIdentifier()) ||
      storage->ContainsGroup(GetSpecificStorageIdentifier());
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
  return true;
}

bool WiFiService::Save(StoreInterface *storage) {
  // Save properties common to all Services.
  if (!Service::Save(storage)) {
    return false;
  }

  // Save properties specific to WiFi services.
  const string id = GetStorageIdentifier();
  storage->SetBool(id, kStorageHiddenSSID, &hidden_ssid_);
  return true;
}

bool WiFiService::IsSecurityMatch(const string &security) const {
  return GetSecurityClass(security) == GetSecurityClass(security_);
}

// private methods
void WiFiService::ConnectTask() {
  std::map<string, DBus::Variant> params;
  DBus::MessageIter writer;

  params[wpa_supplicant::kNetworkPropertyMode].writer().
      append_uint32(WiFiEndpoint::ModeStringToUint(mode_));

  if (security_ == flimflam::kSecurity8021x) {
    NOTIMPLEMENTED();
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
    NOTIMPLEMENTED();
  } else if (security_ == flimflam::kSecurityNone) {
    // Nothing special to do here.
  } else {
    LOG(ERROR) << "Can't connect. Unsupported security method " << security_;
  }

  params[wpa_supplicant::kPropertyKeyManagement].writer().
      append_string(key_management().c_str());
  // TODO(quiche): figure out why we can't use operator<< without the
  // temporary variable.
  writer = params[wpa_supplicant::kNetworkPropertySSID].writer();
  writer << ssid_;

  wifi_->ConnectTo(this, params);
}

string WiFiService::GetDeviceRpcId(Error */*error*/) {
  return wifi_->GetRpcIdentifier();
}

// static
string WiFiService::ParseWEPPassphrase(const string &passphrase, Error *error) {
  unsigned int length = passphrase.length();

  switch (length) {
    case IEEE_80211::kWEP40AsciiLen:
    case IEEE_80211::kWEP104AsciiLen:
      break;
    case IEEE_80211::kWEP40AsciiLen + 2:
    case IEEE_80211::kWEP104AsciiLen + 2:
      CheckWEPKeyIndex(passphrase, error);
      break;
    case IEEE_80211::kWEP40HexLen:
    case IEEE_80211::kWEP104HexLen:
      CheckWEPIsHex(passphrase, error);
      break;
    case IEEE_80211::kWEP40HexLen + 2:
    case IEEE_80211::kWEP104HexLen + 2:
      (CheckWEPKeyIndex(passphrase, error) ||
       CheckWEPPrefix(passphrase, error)) &&
          CheckWEPIsHex(passphrase.substr(2), error);
      break;
    case IEEE_80211::kWEP40HexLen + 4:
    case IEEE_80211::kWEP104HexLen + 4:
      CheckWEPKeyIndex(passphrase, error) &&
          CheckWEPPrefix(passphrase.substr(2), error) &&
          CheckWEPIsHex(passphrase.substr(4), error);
      break;
    default:
      error->Populate(Error::kInvalidPassphrase);
      break;
  }

  // TODO(quiche): may need to normalize passphrase format
  if (error->IsSuccess()) {
    return passphrase;
  } else {
    return "";
  }
}

// static
string WiFiService::ParseWPAPassphrase(const string &passphrase, Error *error) {
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

  // TODO(quiche): may need to normalize passphrase format
  if (error->IsSuccess()) {
    return passphrase;
  } else {
    return "";
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
bool WiFiService::SanitizeSSID(string *ssid) {
  CHECK(ssid);

  size_t ssid_len = ssid->length();
  size_t i;
  bool changed = false;

  for (i=0; i < ssid_len; ++i) {
    if (!g_ascii_isprint((*ssid)[i])) {
      (*ssid)[i] = '?';
      changed = true;
    }
  }

  return changed;
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

}  // namespace shill
