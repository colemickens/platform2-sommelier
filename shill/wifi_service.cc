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

// static
const char WiFiService::kServiceType[] = "wifi";

WiFiService::WiFiService(ControlInterface *control_interface,
                         EventDispatcher *dispatcher,
                         Manager *manager,
                         const WiFiRefPtr &device,
                         const std::vector<uint8_t> ssid,
                         const std::string &mode,
                         const std::string &key_management)
    : Service(control_interface, dispatcher, manager),
      security_(flimflam::kSecurityNone),
      mode_(mode),
      task_factory_(this),
      wifi_(device),
      ssid_(ssid) {
  eap_.key_management = key_management;

  store_.RegisterConstString(flimflam::kModeProperty, &mode_);
  store_.RegisterString(flimflam::kPassphraseProperty, &passphrase_);
  store_.RegisterBool(flimflam::kPassphraseRequiredProperty, &need_passphrase_);
  store_.RegisterConstString(flimflam::kSecurityProperty, &security_);
  store_.RegisterConstUint8(flimflam::kSignalStrengthProperty, &strength_);
  store_.RegisterConstString(flimflam::kTypeProperty, &type_);

  store_.RegisterConstString(flimflam::kWifiAuthMode, &auth_mode_);
  store_.RegisterConstBool(flimflam::kWifiHiddenSsid, &hidden_ssid_);
  store_.RegisterConstUint16(flimflam::kWifiFrequency, &frequency_);
  store_.RegisterConstUint16(flimflam::kWifiPhyMode, &physical_mode_);
  store_.RegisterConstUint16(flimflam::kWifiHexSsid, &hex_ssid_);
}

WiFiService::~WiFiService() {
  LOG(INFO) << __func__;
}

void WiFiService::Connect() {
  LOG(INFO) << __func__;

  // NB(quiche) defer handling, since dbus-c++ does not permit us to
  // send an outbound request while processing an inbound one.
  dispatcher_->PostTask(
      task_factory_.NewRunnableMethod(&WiFiService::ConnectTask));
}

void WiFiService::Disconnect() {
  // TODO(quiche) RemoveNetwork from supplicant
  // XXX remove from favorite networks list?
}

string WiFiService::GetStorageIdentifier(const std::string &mac) {
  string ssid_hex = base::HexEncode(&(*ssid_.begin()), ssid_.size());
  return StringToLowerASCII(base::StringPrintf("%s_%s_%s_%s_%s",
                                               kServiceType,
                                               mac.c_str(),
                                               ssid_hex.c_str(),
                                               mode_.c_str(),
                                               security_.c_str()));
}

const string &WiFiService::mode() const {
  return mode_;
}

const string &WiFiService::key_management() const {
  return eap_.key_management;
}

const std::vector<uint8_t> &WiFiService::ssid() const {
  return ssid_;
}

void WiFiService::ConnectTask() {
  wifi_->ConnectTo(*this);
}

string WiFiService::GetDeviceRpcId() {
  return wifi_->GetRpcIdentifier();
}

}  // namespace shill
