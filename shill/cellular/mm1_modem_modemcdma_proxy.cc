// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/cellular/mm1_modem_modemcdma_proxy.h"

#include <memory>

#include "shill/cellular/cellular_error.h"
#include "shill/dbus_async_call_helper.h"
#include "shill/logging.h"

using std::string;
using std::unique_ptr;

namespace shill {

namespace mm1 {

ModemModemCdmaProxy::ModemModemCdmaProxy(DBus::Connection* connection,
                                         const string& path,
                                         const string& service)
    : proxy_(connection, path, service) {}

ModemModemCdmaProxy::~ModemModemCdmaProxy() {}

void ModemModemCdmaProxy::Activate(const std::string& carrier,
                        Error* error,
                        const ResultCallback& callback,
                        int timeout) {
  BeginAsyncDBusCall(__func__, proxy_, &Proxy::ActivateAsync, callback,
                     error, &CellularError::FromMM1DBusError, timeout,
                     carrier);
}

void ModemModemCdmaProxy::ActivateManual(
    const DBusPropertiesMap& properties,
    Error* error,
    const ResultCallback& callback,
    int timeout) {
  BeginAsyncDBusCall(__func__, proxy_, &Proxy::ActivateManualAsync, callback,
                     error, &CellularError::FromMM1DBusError, timeout,
                     properties);
}

void ModemModemCdmaProxy::set_activation_state_callback(
    const ActivationStateSignalCallback& callback) {
  proxy_.set_activation_state_callback(callback);
}

// ModemModemCdmaProxy::Proxy
ModemModemCdmaProxy::Proxy::Proxy(DBus::Connection* connection,
                                  const std::string& path,
                                  const std::string& service)
    : DBus::ObjectProxy(*connection, path, service.c_str()) {}

ModemModemCdmaProxy::Proxy::~Proxy() {}

void ModemModemCdmaProxy::Proxy::set_activation_state_callback(
    const ActivationStateSignalCallback& callback) {
  activation_state_callback_ = callback;
}

// Signal callbacks inherited from Proxy
void ModemModemCdmaProxy::Proxy::ActivationStateChanged(
    const uint32_t& activation_state,
    const uint32_t& activation_error,
    const DBusPropertiesMap& status_changes) {
  SLOG(&path(), 2) << __func__;
  activation_state_callback_.Run(activation_state,
                                 activation_error,
                                 status_changes);
}

// Method callbacks inherited from
// org::freedesktop::ModemManager1::Modem::ModemModemCdmaProxy
void ModemModemCdmaProxy::Proxy::ActivateCallback(const ::DBus::Error& dberror,
                                                  void* data) {
  SLOG(&path(), 2) << __func__;
  unique_ptr<ResultCallback> callback(reinterpret_cast<ResultCallback*>(data));
  Error error;
  CellularError::FromMM1DBusError(dberror, &error);
  callback->Run(error);
}

void ModemModemCdmaProxy::Proxy::ActivateManualCallback(
    const ::DBus::Error& dberror,
    void* data) {
  SLOG(&path(), 2) << __func__;
  unique_ptr<ResultCallback> callback(reinterpret_cast<ResultCallback*>(data));
  Error error;
  CellularError::FromMM1DBusError(dberror, &error);
  callback->Run(error);
}

}  // namespace mm1
}  // namespace shill
