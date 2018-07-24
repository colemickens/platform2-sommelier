// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/cellular/modem_proxy.h"

#include <memory>

#include <base/bind.h>
#include <base/strings/stringprintf.h>

#include "shill/cellular/cellular_error.h"
#include "shill/dbus_async_call_helper.h"
#include "shill/error.h"
#include "shill/logging.h"

using base::Bind;
using base::Callback;
using base::StringPrintf;
using std::string;
using std::unique_ptr;

namespace shill {

typedef Callback<void(const ModemHardwareInfo&,
                      const Error&)> ModemInfoCallback;

ModemProxy::ModemProxy(DBus::Connection* connection,
                       const string& path,
                       const string& service)
    : proxy_(connection, path, service) {}

ModemProxy::~ModemProxy() {}

void ModemProxy::set_state_changed_callback(
      const ModemStateChangedSignalCallback& callback) {
  proxy_.set_state_changed_callback(callback);
}

void ModemProxy::Enable(bool enable, Error* error,
                        const ResultCallback& callback, int timeout) {
  BeginAsyncDBusCall(StringPrintf("%s(%s)", __func__,
                                  enable ? "true" : "false"),
                     proxy_, &Proxy::EnableAsync, callback, error,
                     &CellularError::FromDBusError, timeout, enable);
}

void ModemProxy::Disconnect(Error* error, const ResultCallback& callback,
                            int timeout) {
  BeginAsyncDBusCall(__func__, proxy_, &Proxy::DisconnectAsync, callback,
                     error, &CellularError::FromDBusError, timeout);
}

void ModemProxy::GetModemInfo(Error* error,
                              const ModemInfoCallback& callback,
                              int timeout) {
  BeginAsyncDBusCall(__func__, proxy_, &Proxy::GetInfoAsync, callback,
                     error, &CellularError::FromDBusError, timeout);
}

ModemProxy::Proxy::Proxy(DBus::Connection* connection,
                         const string& path,
                         const string& service)
    : DBus::ObjectProxy(*connection, path, service.c_str()) {}

ModemProxy::Proxy::~Proxy() {}

void ModemProxy::Proxy::set_state_changed_callback(
      const ModemStateChangedSignalCallback& callback) {
  state_changed_callback_ = callback;
}

void ModemProxy::Proxy::StateChanged(
    const uint32_t& old, const uint32_t &_new, const uint32_t& reason) {
  SLOG(&path(), 2) << __func__ << "(" << old << ", " << _new << ", "
                   << reason << ")";
  state_changed_callback_.Run(old, _new, reason);
}

void ModemProxy::Proxy::EnableCallback(const DBus::Error& dberror, void* data) {
  SLOG(&path(), 2) << __func__;
  unique_ptr<ResultCallback> callback(reinterpret_cast<ResultCallback*>(data));
  Error error;
  CellularError::FromDBusError(dberror, &error);
  callback->Run(error);
}

void ModemProxy::Proxy::GetInfoCallback(const ModemHardwareInfo& info,
                                        const DBus::Error& dberror,
                                        void* data) {
  SLOG(&path(), 2) << __func__;
  unique_ptr<ModemInfoCallback> callback(
      reinterpret_cast<ModemInfoCallback*>(data));
  Error error;
  CellularError::FromDBusError(dberror, &error);
  callback->Run(info, error);
}

void ModemProxy::Proxy::DisconnectCallback(const DBus::Error& dberror,
                                           void* data) {
  SLOG(&path(), 2) << __func__;
  unique_ptr<ResultCallback> callback(reinterpret_cast<ResultCallback*>(data));
  Error error;
  CellularError::FromDBusError(dberror, &error);
  callback->Run(error);
}

}  // namespace shill
