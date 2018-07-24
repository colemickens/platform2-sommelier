// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/cellular/modem_simple_proxy.h"

#include <memory>

#include <base/bind.h>

#include "shill/cellular/cellular_error.h"
#include "shill/dbus_async_call_helper.h"
#include "shill/error.h"
#include "shill/logging.h"

using base::Bind;
using base::Callback;
using std::string;
using std::unique_ptr;

namespace shill {

typedef Callback<void(const DBusPropertiesMap&,
                      const Error&)> ModemStatusCallback;

ModemSimpleProxy::ModemSimpleProxy(DBus::Connection* connection,
                                   const string& path,
                                   const string& service)
    : proxy_(connection, path, service) {}

ModemSimpleProxy::~ModemSimpleProxy() {}

void ModemSimpleProxy::GetModemStatus(Error* error,
                                      const DBusPropertyMapCallback& callback,
                                      int timeout) {
  BeginAsyncDBusCall(__func__, proxy_, &Proxy::GetStatusAsync, callback,
                     error, &CellularError::FromDBusError, timeout);
}

void ModemSimpleProxy::Connect(const DBusPropertiesMap& properties,
                               Error* error,
                               const ResultCallback& callback,
                               int timeout) {
  BeginAsyncDBusCall(__func__, proxy_, &Proxy::ConnectAsync, callback,
                     error, &CellularError::FromDBusError, timeout,
                     properties);
}

ModemSimpleProxy::Proxy::Proxy(DBus::Connection* connection,
                               const string& path,
                               const string& service)
    : DBus::ObjectProxy(*connection, path, service.c_str()) {}

ModemSimpleProxy::Proxy::~Proxy() {}

void ModemSimpleProxy::Proxy::GetStatusCallback(const DBusPropertiesMap& props,
                                                const DBus::Error& dberror,
                                                void* data) {
  SLOG(&path(), 2) << __func__;
  unique_ptr<DBusPropertyMapCallback> callback(
      reinterpret_cast<DBusPropertyMapCallback*>(data));
  Error error;
  CellularError::FromDBusError(dberror, &error);
  callback->Run(props, error);
}

void ModemSimpleProxy::Proxy::ConnectCallback(const DBus::Error& dberror,
                                              void* data) {
  SLOG(&path(), 2) << __func__;
  unique_ptr<ResultCallback> callback(reinterpret_cast<ResultCallback*>(data));
  Error error;
  CellularError::FromDBusError(dberror, &error);
  callback->Run(error);
}

}  // namespace shill
