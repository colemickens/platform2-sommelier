// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/modem_gsm_network_proxy.h"

#include <base/logging.h>

using std::string;

namespace shill {

ModemGSMNetworkProxy::ModemGSMNetworkProxy(
    ModemGSMNetworkProxyListener *listener,
    DBus::Connection *connection,
    const string &path,
    const string &service)
    : proxy_(listener, connection, path, service) {}

ModemGSMNetworkProxy::~ModemGSMNetworkProxy() {}

void ModemGSMNetworkProxy::Register(const string &network_id) {
  proxy_.Register(network_id);
}

ModemGSMNetworkProxy::Proxy::Proxy(ModemGSMNetworkProxyListener *listener,
                                   DBus::Connection *connection,
                                   const string &path,
                                   const string &service)
    : DBus::ObjectProxy(*connection, path, service.c_str()),
      listener_(listener) {}

ModemGSMNetworkProxy::Proxy::~Proxy() {}

void ModemGSMNetworkProxy::Proxy::SignalQuality(const uint32 &quality) {
  VLOG(2) << __func__ << "(" << quality << ")";
  listener_->OnGSMSignalQualityChanged(quality);
}

void ModemGSMNetworkProxy::Proxy::RegistrationInfo(
    const uint32_t &status,
    const string &operator_code,
    const string &operator_name) {
  VLOG(2) << __func__ << "(" << status << ", " << operator_code << ", "
          << operator_name << ")";
  listener_->OnGSMRegistrationInfoChanged(status, operator_code, operator_name);
}

void ModemGSMNetworkProxy::Proxy::NetworkMode(const uint32_t &mode) {
  VLOG(2) << __func__ << "(" << mode << ")";
  listener_->OnGSMNetworkModeChanged(mode);
}

}  // namespace shill
