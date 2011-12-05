// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/modem_cdma_proxy.h"

#include <base/logging.h>

#include "cellular_error.h"

using std::string;

namespace shill {

ModemCDMAProxy::ModemCDMAProxy(ModemCDMAProxyDelegate *delegate,
                               DBus::Connection *connection,
                               const string &path,
                               const string &service)
    : proxy_(delegate, connection, path, service) {}

ModemCDMAProxy::~ModemCDMAProxy() {}

void ModemCDMAProxy::Activate(const string &carrier,
                                AsyncCallHandler *call_handler, int timeout) {
  proxy_.Activate(carrier, call_handler, timeout);
}

void ModemCDMAProxy::GetRegistrationState(uint32 *cdma_1x_state,
                                          uint32 *evdo_state) {
  proxy_.GetRegistrationState(*cdma_1x_state, *evdo_state);
}

uint32 ModemCDMAProxy::GetSignalQuality() {
  return proxy_.GetSignalQuality();
}

const string ModemCDMAProxy::MEID() {
  LOG(INFO) << "ModemCDMAProxy::" << __func__;
  return proxy_.Meid();
}

ModemCDMAProxy::Proxy::Proxy(ModemCDMAProxyDelegate *delegate,
                             DBus::Connection *connection,
                             const string &path,
                             const string &service)
    : DBus::ObjectProxy(*connection, path, service.c_str()),
      delegate_(delegate) {}

ModemCDMAProxy::Proxy::~Proxy() {}

void ModemCDMAProxy::Proxy::ActivationStateChanged(
    const uint32 &activation_state,
    const uint32 &activation_error,
    const DBusPropertiesMap &status_changes) {
  VLOG(2) << __func__ << "(" << activation_state << ", " << activation_error
          << ")";
  delegate_->OnCDMAActivationStateChanged(
      activation_state, activation_error, status_changes);
}

void ModemCDMAProxy::Proxy::SignalQuality(const uint32 &quality) {
  VLOG(2) << __func__ << "(" << quality << ")";
  delegate_->OnCDMASignalQualityChanged(quality);
}

void ModemCDMAProxy::Proxy::RegistrationStateChanged(
    const uint32 &cdma_1x_state,
    const uint32 &evdo_state) {
  VLOG(2) << __func__ << "(" << cdma_1x_state << ", " << evdo_state << ")";
  delegate_->OnCDMARegistrationStateChanged(cdma_1x_state, evdo_state);
}

void ModemCDMAProxy::Proxy::ActivateCallback(const uint32 &status,
                                             const DBus::Error &dberror,
                                             void *data) {
  VLOG(2) << __func__ << "(" << status << ")";
  AsyncCallHandler *call_handler = reinterpret_cast<AsyncCallHandler *>(data);
  Error error;
  CellularError::FromDBusError(dberror, &error),
  delegate_->OnActivateCallback(status, error, call_handler);
}

}  // namespace shill
