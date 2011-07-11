// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/wifi_service.h"

#include <string>

#include <base/logging.h>
#include <chromeos/dbus/service_constants.h>

#include "shill/control_interface.h"
#include "shill/device.h"
#include "shill/entry.h"
#include "shill/shill_event.h"
#include "shill/wifi.h"

using std::string;

namespace shill {

WiFiService::WiFiService(ControlInterface *control_interface,
                         EventDispatcher *dispatcher,
                         const WiFiRefPtr &device,
                         const ProfileRefPtr &profile,
                         const EntryRefPtr &entry,
                         const std::vector<uint8_t> ssid,
                         uint32_t mode,
                         const std::string &key_management,
                         const std::string &name)
    : Service(control_interface, dispatcher, profile, entry, name),
      task_factory_(this),
      wifi_(device),
      ssid_(ssid),
      mode_(mode) {
  entry_->eap.key_management = key_management;

  // TODO(cmasone): Figure out if mode_ should be a string or what
  // store_.RegisterString(flimflam::kModeProperty, &entry_->mode);
  store_.RegisterString(flimflam::kPassphraseProperty, &passphrase_);
  store_.RegisterBool(flimflam::kPassphraseRequiredProperty, &need_passphrase_);
  store_.RegisterConstString(flimflam::kSecurityProperty, &entry_->security);
  store_.RegisterConstUint8(flimflam::kSignalStrengthProperty, &strength_);
  store_.RegisterConstString(flimflam::kTypeProperty, &type_);

  store_.RegisterConstString(flimflam::kWifiAuthMode, &auth_mode_);
  store_.RegisterConstBool(flimflam::kWifiHiddenSsid, &entry_->hidden_ssid);
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
      task_factory_.NewRunnableMethod(&WiFiService::RealConnect));
}

void WiFiService::Disconnect() {
  // TODO(quiche) RemoveNetwork from supplicant
  // XXX remove from favorite networks list?
}

uint32_t WiFiService::mode() const {
  return mode_;
}

const std::string &WiFiService::key_management() const {
  return entry_->eap.key_management;
}

const std::vector<uint8_t> &WiFiService::ssid() const {
  return ssid_;
}

void WiFiService::RealConnect() {
  wifi_->ConnectTo(*this);
}

string WiFiService::GetDeviceRpcId() {
  return wifi_->GetRpcIdentifier();
}

}  // namespace shill
