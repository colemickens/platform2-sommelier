// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/modem_proxy.h"

#include <base/logging.h>

#include "shill/cellular_error.h"
#include "shill/error.h"

using std::string;

namespace shill {

ModemProxy::ModemProxy(ModemProxyDelegate *delegate,
                       DBus::Connection *connection,
                       const string &path,
                       const string &service)
    : proxy_(delegate, connection, path, service) {}

ModemProxy::~ModemProxy() {}

// TODO(ers): Need to handle dbus errors that occur and prevent the
// RPC from going out. These currently result in exceptions being thrown.
// We need a way to let the upper layers know that the operation
// failed in such a case.
void ModemProxy::Enable(bool enable, AsyncCallHandler *call_handler,
                        int timeout) {
  VLOG(2) << __func__ << "(" << enable << ", " << timeout << ")";
  proxy_.Enable(enable, call_handler, timeout);
}

// TODO(ers): temporarily support the blocking version
// of Enable, until Cellular::Stop is converted for async.
void ModemProxy::Enable(bool enable) {
  VLOG(2) << __func__ << "(" << enable << ")";
  proxy_.Enable(enable);
}

void ModemProxy::Disconnect() {
  proxy_.Disconnect();
}

void ModemProxy::GetModemInfo(AsyncCallHandler *call_handler, int timeout) {
  proxy_.GetInfo(call_handler, timeout);
}

ModemProxy::Proxy::Proxy(ModemProxyDelegate *delegate,
                         DBus::Connection *connection,
                         const string &path,
                         const string &service)
    : DBus::ObjectProxy(*connection, path, service.c_str()),
      delegate_(delegate) {}

ModemProxy::Proxy::~Proxy() {}

void ModemProxy::Proxy::StateChanged(
    const uint32 &old, const uint32 &_new, const uint32 &reason) {
  VLOG(2) << __func__ << "(" << old << ", " << _new << ", " << reason << ")";
  delegate_->OnModemStateChanged(old, _new, reason);
}

void ModemProxy::Proxy::EnableCallback(const DBus::Error &dberror, void *data) {
  VLOG(2) << __func__;
  AsyncCallHandler *call_handler = reinterpret_cast<AsyncCallHandler *>(data);
  Error error;
  CellularError::FromDBusError(dberror, &error);
  delegate_->OnModemEnableCallback(error, call_handler);
}

void ModemProxy::Proxy::GetInfoCallback(const ModemHardwareInfo &info,
                                        const DBus::Error &dberror,
                                        void *data) {
  AsyncCallHandler *call_handler = reinterpret_cast<AsyncCallHandler *>(data);
  Error error;
  CellularError::FromDBusError(dberror, &error);
  delegate_->OnGetModemInfoCallback(info, error, call_handler);
}

void ModemProxy::Proxy::DisconnectCallback(const DBus::Error &dberror,
                                           void *data) {
  AsyncCallHandler *call_handler = reinterpret_cast<AsyncCallHandler *>(data);
  Error error;
  CellularError::FromDBusError(dberror, &error);
  delegate_->OnDisconnectCallback(error, call_handler);
}

}  // namespace shill
