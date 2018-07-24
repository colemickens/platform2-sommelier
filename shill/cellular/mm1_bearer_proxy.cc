// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/cellular/mm1_bearer_proxy.h"

#include <memory>

#include "shill/cellular/cellular_error.h"
#include "shill/dbus_async_call_helper.h"
#include "shill/logging.h"

using std::string;
using std::unique_ptr;

namespace shill {

namespace mm1 {

BearerProxy::BearerProxy(DBus::Connection* connection,
                         const std::string& path,
                         const std::string& service)
    : proxy_(connection, path, service) {}

BearerProxy::~BearerProxy() {}


// Inherited methods from BearerProxyInterface
void BearerProxy::Connect(Error* error,
                          const ResultCallback& callback,
                          int timeout) {
  BeginAsyncDBusCall(__func__, proxy_, &Proxy::ConnectAsync, callback,
                     error, &CellularError::FromMM1DBusError, timeout);
}

void BearerProxy::Disconnect(Error* error,
                             const ResultCallback& callback,
                             int timeout) {
  BeginAsyncDBusCall(__func__, proxy_, &Proxy::DisconnectAsync, callback,
                     error, &CellularError::FromMM1DBusError, timeout);
}

BearerProxy::Proxy::Proxy(DBus::Connection* connection,
                          const std::string& path,
                          const std::string& service)
    : DBus::ObjectProxy(*connection, path, service.c_str()) {}

BearerProxy::Proxy::~Proxy() {}

// Method callbacks inherited from
// org::freedesktop::ModemManager1::BearerProxy
void BearerProxy::Proxy::ConnectCallback(const ::DBus::Error& dberror,
                                         void* data) {
  SLOG(&path(), 2) << __func__;
  unique_ptr<ResultCallback> callback(reinterpret_cast<ResultCallback*>(data));
  Error error;
  CellularError::FromMM1DBusError(dberror, &error);
  callback->Run(error);
}

void BearerProxy::Proxy::DisconnectCallback(const ::DBus::Error& dberror,
                                            void* data) {
  SLOG(&path(), 2) << __func__;
  unique_ptr<ResultCallback> callback(reinterpret_cast<ResultCallback*>(data));
  Error error;
  CellularError::FromMM1DBusError(dberror, &error);
  callback->Run(error);
}

}  // namespace mm1
}  // namespace shill
