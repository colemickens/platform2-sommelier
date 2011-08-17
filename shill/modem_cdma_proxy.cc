// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/modem_cdma_proxy.h"

#include <base/logging.h>

using std::string;

namespace shill {

ModemCDMAProxy::ModemCDMAProxy(ModemCDMAProxyListener *listener,
                               DBus::Connection *connection,
                               const string &path,
                               const string &service)
    : proxy_(listener, connection, path, service) {}

ModemCDMAProxy::~ModemCDMAProxy() {}

uint32 ModemCDMAProxy::Activate(const string &carrier) {
  return proxy_.Activate(carrier);
}

void ModemCDMAProxy::GetRegistrationState(uint32 *cdma_1x_state,
                                          uint32 *evdo_state) {
  proxy_.GetRegistrationState(*cdma_1x_state, *evdo_state);
}

uint32 ModemCDMAProxy::GetSignalQuality() {
  return proxy_.GetSignalQuality();
}

ModemCDMAProxy::Proxy::Proxy(ModemCDMAProxyListener *listener,
                             DBus::Connection *connection,
                             const string &path,
                             const string &service)
    : DBus::ObjectProxy(*connection, path, service.c_str()),
      listener_(listener) {}

ModemCDMAProxy::Proxy::~Proxy() {}

void ModemCDMAProxy::Proxy::ActivationStateChanged(
    const uint32 &activation_state,
    const uint32 &activation_error,
    const DBusPropertiesMap &status_changes) {
  VLOG(2) << __func__ << "(" << activation_state << ", " << activation_error
          << ")";
  listener_->OnCDMAActivationStateChanged(
      activation_state, activation_error, status_changes);
}

void ModemCDMAProxy::Proxy::SignalQuality(const uint32 &quality) {
  VLOG(2) << __func__ << "(" << quality << ")";
  listener_->OnCDMASignalQualityChanged(quality);
}

void ModemCDMAProxy::Proxy::RegistrationStateChanged(
    const uint32 &cdma_1x_state,
    const uint32 &evdo_state) {
  VLOG(2) << __func__ << "(" << cdma_1x_state << ", " << evdo_state << ")";
  listener_->OnCDMARegistrationStateChanged(cdma_1x_state, evdo_state);
}

}  // namespace shill
