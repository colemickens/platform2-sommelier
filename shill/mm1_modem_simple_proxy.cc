// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/mm1_modem_simple_proxy.h"

#include <base/logging.h>

#include "cellular_error.h"
#include "async_call_handler.h"

using std::string;

namespace shill {
namespace mm1 {

ModemSimpleProxy::ModemSimpleProxy(ModemSimpleProxyDelegate *delegate,
                                   DBus::Connection *connection,
                                   const string &path,
                                   const string &service)
    : proxy_(delegate, connection, path, service) {}

ModemSimpleProxy::~ModemSimpleProxy() {}

void ModemSimpleProxy::Connect(
    const DBusPropertiesMap &properties,
    AsyncCallHandler *call_handler,
    int timeout) {
  proxy_.Connect(properties, call_handler, timeout);
}

void ModemSimpleProxy::Disconnect(const ::DBus::Path &bearer,
                                  AsyncCallHandler *call_handler,
                                  int timeout) {
  proxy_.Disconnect(bearer, call_handler, timeout);
}

void ModemSimpleProxy::GetStatus(AsyncCallHandler *call_handler, int timeout) {
  proxy_.GetStatus(call_handler, timeout);
}


// ModemSimpleProxy::Proxy
ModemSimpleProxy::Proxy::Proxy(ModemSimpleProxyDelegate *delegate,
                               DBus::Connection *connection,
                               const std::string &path,
                               const std::string &service)
    : DBus::ObjectProxy(*connection, path, service.c_str()),
      delegate_(delegate) {}

ModemSimpleProxy::Proxy::~Proxy() {}

// Method callbacks inherited from
// org::freedesktop::ModemManager1::Modem::ModemSimpleProxy
void ModemSimpleProxy::Proxy::ConnectCallback(const ::DBus::Path &bearer,
                                              const ::DBus::Error &dberror,
                                              void *data) {
  AsyncCallHandler *call_handler = reinterpret_cast<AsyncCallHandler *>(data);
  Error error;
  CellularError::FromDBusError(dberror, &error),
      delegate_->OnConnectCallback(bearer, error, call_handler);
}

void ModemSimpleProxy::Proxy::DisconnectCallback(const ::DBus::Error &dberror,
                                                 void *data) {
  AsyncCallHandler *call_handler = reinterpret_cast<AsyncCallHandler *>(data);
  Error error;
  CellularError::FromDBusError(dberror, &error),
      delegate_->OnDisconnectCallback(error, call_handler);
}

void ModemSimpleProxy::Proxy::GetStatusCallback(
    const DBusPropertiesMap &properties,
    const ::DBus::Error &dberror,
    void *data) {
  AsyncCallHandler *call_handler = reinterpret_cast<AsyncCallHandler *>(data);
  Error error;
  CellularError::FromDBusError(dberror, &error),
      delegate_->OnGetStatusCallback(properties, error, call_handler);
}

}  // namespace mm1
}  // namespace shill
