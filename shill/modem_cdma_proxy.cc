// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/modem_cdma_proxy.h"

#include <base/logging.h>

using std::string;

namespace shill {

ModemCDMAProxy::ModemCDMAProxy(DBus::Connection *connection,
                               const string &path,
                               const string &service)
    : proxy_(connection, path, service) {}

ModemCDMAProxy::~ModemCDMAProxy() {}

void ModemCDMAProxy::GetRegistrationState(uint32 *cdma_1x_state,
                                          uint32 *evdo_state) {
  proxy_.GetRegistrationState(*cdma_1x_state, *evdo_state);
}

ModemCDMAProxy::Proxy::Proxy(DBus::Connection *connection,
                             const string &path,
                             const string &service)
    : DBus::ObjectProxy(*connection, path, service.c_str()) {}

ModemCDMAProxy::Proxy::~Proxy() {}

void ModemCDMAProxy::Proxy::ActivationStateChanged(
    const uint32 &activation_state,
    const uint32 &activation_error,
    const DBusPropertiesMap &status_changes) {
  VLOG(2) << __func__;
  // TODO(petkov): Implement this.
  NOTIMPLEMENTED();
}

void ModemCDMAProxy::Proxy::SignalQuality(const uint32 &quality) {
  VLOG(2) << __func__;
  // TODO(petkov): Implement this.
  NOTIMPLEMENTED();
}

void ModemCDMAProxy::Proxy::RegistrationStateChanged(
    const uint32_t &cdma_1x_state,
    const uint32_t &evdo_state) {
  VLOG(2) << __func__;
  // TODO(petkov): Implement this.
  NOTIMPLEMENTED();
}

}  // namespace shill
