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

#include "shill/control_interface.h"
#include "shill/device.h"
#include "shill/shill_event.h"
#include "shill/wifi.h"

using std::string;

namespace shill {

WiFiService::WiFiService(ControlInterface *control_interface,
                         EventDispatcher *dispatcher,
                         Manager *manager,
                         const WiFiRefPtr &device,
                         const std::vector<uint8_t> ssid,
                         const std::string &mode,
                         const std::string &key_management)
    : Service(control_interface, dispatcher, manager, flimflam::kTypeWifi),
      security_(flimflam::kSecurityNone),
      mode_(mode),
      task_factory_(this),
      wifi_(device),
      ssid_(ssid) {
  SetEAPKeyManagement(key_management);

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

  // TODO(quiche): set based on security properties
  need_passphrase_ = false;
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
  wifi_->ConnectTo(this);
}

string WiFiService::GetDeviceRpcId() {
  return wifi_->GetRpcIdentifier();
}

}  // namespace shill
