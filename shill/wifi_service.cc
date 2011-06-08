// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/wifi_service.h"

#include <string>

#include <base/logging.h>

#include "shill/control_interface.h"
#include "shill/device.h"
#include "shill/shill_event.h"
#include "shill/wifi.h"

using std::string;

namespace shill {
const char WiFiService::kSupplicantPropertySSID[]        = "ssid";
const char WiFiService::kSupplicantPropertyNetworkMode[] = "mode";
const char WiFiService::kSupplicantPropertyKeyMode[]     = "key_mgmt";

WiFiService::WiFiService(ControlInterface *control_interface,
                         EventDispatcher *dispatcher,
                         WiFi *device,
                         const std::vector<uint8_t> ssid,
                         uint32_t mode,
                         const std::string &key_management,
                         const std::string &name)
    : Service(control_interface, dispatcher, device, name),
      task_factory_(this),
      wifi_(device),
      ssid_(ssid),
      mode_(mode),
      key_management_(key_management) {
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

void WiFiService::RealConnect() {
  std::map<string, DBus::Variant> add_network_args;
  DBus::MessageIter mi;
  DBus::Path network_path;

  add_network_args[kSupplicantPropertyNetworkMode].writer().
      append_uint32(mode_);
  add_network_args[kSupplicantPropertyKeyMode].writer().
      append_string(key_management_.c_str());
  // TODO(quiche): figure out why we can't use operator<< without the
  // temporary variable.
  mi = add_network_args[kSupplicantPropertySSID].writer();
  mi << ssid_;
  // TODO(quiche): set scan_ssid=1, like flimflam does?

  network_path = wifi_->AddNetwork(add_network_args);
  wifi_->SelectNetwork(network_path);
  // XXX add to favorite networks list?
}

void WiFiService::Disconnect() {
  // TODO(quiche) RemoveNetwork from supplicant
  // XXX remove from favorite networks list?
}

}  // namespace shill
