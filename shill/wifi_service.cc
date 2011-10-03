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

#include "shill/control_interface.h"
#include "shill/device.h"
#include "shill/shill_event.h"
#include "shill/wifi.h"
#include "shill/wifi_endpoint.h"
#include "shill/wpa_supplicant.h"

using std::string;

namespace shill {

WiFiService::WiFiService(ControlInterface *control_interface,
                         EventDispatcher *dispatcher,
                         Manager *manager,
                         const WiFiRefPtr &device,
                         const std::vector<uint8_t> ssid,
                         const std::string &mode,
                         const std::string &security)
    : Service(control_interface, dispatcher, manager, flimflam::kTypeWifi),
      security_(security),
      mode_(mode),
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
  store->RegisterConstString(flimflam::kWifiHexSsid, &hex_ssid_);

  hex_ssid_ = base::HexEncode(&(*ssid_.begin()), ssid_.size());
  set_name(name() +
           "_" +
           string(reinterpret_cast<const char*>(ssid_.data()), ssid_.size()));

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

  // TODO(quiche): figure out when to set true
  hidden_ssid_ = false;
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

string WiFiService::GetStorageIdentifier() {
  return StringToLowerASCII(base::StringPrintf("%s_%s_%s_%s_%s",
                                               flimflam::kTypeWifi,
                                               wifi_->address().c_str(),
                                               hex_ssid_.c_str(),
                                               mode_.c_str(),
                                               security_.c_str()));
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

// private methods
void WiFiService::ConnectTask() {
  std::map<string, DBus::Variant> params;
  DBus::MessageIter writer;

  params[wpa_supplicant::kNetworkPropertyMode].writer().
      append_uint32(WiFiEndpoint::ModeStringToUint(mode_));

  if (security_ == flimflam::kSecurity8021x) {
    NOTIMPLEMENTED();
  } else if (security_ == flimflam::kSecurityPsk) {
    NOTIMPLEMENTED();
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
    // nothing special to do here
  } else {
    LOG(ERROR) << "can't connect. unsupported security method " << security_;
  }

  params[wpa_supplicant::kPropertyKeyManagement].writer().
      append_string(key_management().c_str());
  // TODO(quiche): figure out why we can't use operator<< without the
  // temporary variable.
  writer = params[wpa_supplicant::kNetworkPropertySSID].writer();
  writer << ssid_;

  wifi_->ConnectTo(this, params);
}

string WiFiService::GetDeviceRpcId() {
  return wifi_->GetRpcIdentifier();
}

}  // namespace shill
