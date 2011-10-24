// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/modem_gsm_network_proxy.h"

#include <base/logging.h>

using std::string;

namespace shill {

ModemGSMNetworkProxy::ModemGSMNetworkProxy(
    ModemGSMNetworkProxyDelegate *delegate,
    DBus::Connection *connection,
    const string &path,
    const string &service)
    : proxy_(delegate, connection, path, service) {}

ModemGSMNetworkProxy::~ModemGSMNetworkProxy() {}

ModemGSMNetworkProxyInterface::RegistrationInfo
ModemGSMNetworkProxy::GetRegistrationInfo() {
  return proxy_.GetRegistrationInfo();
}

uint32 ModemGSMNetworkProxy::GetSignalQuality() {
  return proxy_.GetSignalQuality();
}

void ModemGSMNetworkProxy::Register(const string &network_id) {
  proxy_.Register(network_id);
}

ModemGSMNetworkProxyInterface::ScanResults ModemGSMNetworkProxy::Scan() {
  return proxy_.Scan();
}

uint32 ModemGSMNetworkProxy::AccessTechnology() {
  return proxy_.AccessTechnology();
}

ModemGSMNetworkProxy::Proxy::Proxy(ModemGSMNetworkProxyDelegate *delegate,
                                   DBus::Connection *connection,
                                   const string &path,
                                   const string &service)
    : DBus::ObjectProxy(*connection, path, service.c_str()),
      delegate_(delegate) {}

ModemGSMNetworkProxy::Proxy::~Proxy() {}

void ModemGSMNetworkProxy::Proxy::SignalQuality(const uint32 &quality) {
  VLOG(2) << __func__ << "(" << quality << ")";
  delegate_->OnGSMSignalQualityChanged(quality);
}

void ModemGSMNetworkProxy::Proxy::RegistrationInfo(
    const uint32_t &status,
    const string &operator_code,
    const string &operator_name) {
  VLOG(2) << __func__ << "(" << status << ", " << operator_code << ", "
          << operator_name << ")";
  delegate_->OnGSMRegistrationInfoChanged(status, operator_code, operator_name);
}

void ModemGSMNetworkProxy::Proxy::NetworkMode(const uint32_t &mode) {
  VLOG(2) << __func__ << "(" << mode << ")";
  delegate_->OnGSMNetworkModeChanged(mode);
}

}  // namespace shill
