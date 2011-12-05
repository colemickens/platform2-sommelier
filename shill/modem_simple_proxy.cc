// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/modem_simple_proxy.h"

#include "shill/cellular_error.h"
#include "shill/error.h"

using std::string;

namespace shill {

ModemSimpleProxy::ModemSimpleProxy(ModemSimpleProxyDelegate *delegate,
                                   DBus::Connection *connection,
                                   const string &path,
                                   const string &service)
    : proxy_(delegate, connection, path, service) {}

ModemSimpleProxy::~ModemSimpleProxy() {}

void ModemSimpleProxy::GetModemStatus(AsyncCallHandler *call_handler,
                                      int timeout) {
  proxy_.GetStatus(call_handler, timeout);
}

void ModemSimpleProxy::Connect(const DBusPropertiesMap &properties,
                               AsyncCallHandler *call_handler, int timeout) {
  proxy_.Connect(properties, call_handler, timeout);
}

ModemSimpleProxy::Proxy::Proxy(ModemSimpleProxyDelegate *delegate,
                               DBus::Connection *connection,
                               const string &path,
                               const string &service)
    : DBus::ObjectProxy(*connection, path, service.c_str()),
      delegate_(delegate) {}

ModemSimpleProxy::Proxy::~Proxy() {}

void ModemSimpleProxy::Proxy::GetStatusCallback(const DBusPropertiesMap &props,
                                                const DBus::Error &dberror,
                                                void *data) {
  AsyncCallHandler *call_handler = reinterpret_cast<AsyncCallHandler *>(data);
  Error error;
  CellularError::FromDBusError(dberror, &error);
  delegate_->OnGetModemStatusCallback(props, error, call_handler);
}

void ModemSimpleProxy::Proxy::ConnectCallback(const DBus::Error &dberror,
                                              void *data) {
  AsyncCallHandler *call_handler = reinterpret_cast<AsyncCallHandler *>(data);
  Error error;
  CellularError::FromDBusError(dberror, &error);
  delegate_->OnConnectCallback(error, call_handler);
}

}  // namespace shill
