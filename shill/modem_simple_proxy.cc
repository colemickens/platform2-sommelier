// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/modem_simple_proxy.h"

#include <base/bind.h>

#include "shill/cellular_error.h"
#include "shill/error.h"
#include "shill/scope_logger.h"

using base::Bind;
using base::Callback;
using std::string;

namespace shill {

typedef Callback<void(const DBusPropertiesMap &,
                      const Error &)> ModemStatusCallback;

ModemSimpleProxy::ModemSimpleProxy(DBus::Connection *connection,
                                   const string &path,
                                   const string &service)
    : proxy_(connection, path, service) {}

ModemSimpleProxy::~ModemSimpleProxy() {}

void ModemSimpleProxy::GetModemStatus(Error *error,
                                      const DBusPropertyMapCallback &callback,
                                      int timeout) {
  scoped_ptr<DBusPropertyMapCallback> cb(new DBusPropertyMapCallback(callback));
  try {
    SLOG(DBus, 2) << __func__;
    proxy_.GetStatus(cb.get(), timeout);
    cb.release();
  } catch (const DBus::Error &e) {
    if (error)
      CellularError::FromDBusError(e, error);
  }
}

void ModemSimpleProxy::Connect(const DBusPropertiesMap &properties,
                               Error *error,
                               const ResultCallback &callback,
                               int timeout) {
  scoped_ptr<ResultCallback> cb(new ResultCallback(callback));
  try {
    SLOG(DBus, 2) << __func__;
    proxy_.Connect(properties, cb.get(), timeout);
    cb.release();
  } catch (const DBus::Error &e) {
    if (error)
      CellularError::FromDBusError(e, error);
  }
}

ModemSimpleProxy::Proxy::Proxy(DBus::Connection *connection,
                               const string &path,
                               const string &service)
    : DBus::ObjectProxy(*connection, path, service.c_str()) {}

ModemSimpleProxy::Proxy::~Proxy() {}

void ModemSimpleProxy::Proxy::GetStatusCallback(const DBusPropertiesMap &props,
                                                const DBus::Error &dberror,
                                                void *data) {
  SLOG(DBus, 2) << __func__;
  scoped_ptr<DBusPropertyMapCallback> callback(
      reinterpret_cast<DBusPropertyMapCallback *>(data));
  Error error;
  CellularError::FromDBusError(dberror, &error);
  callback->Run(props, error);
}

void ModemSimpleProxy::Proxy::ConnectCallback(const DBus::Error &dberror,
                                              void *data) {
  SLOG(DBus, 2) << __func__;
  scoped_ptr<ResultCallback> callback(reinterpret_cast<ResultCallback *>(data));
  Error error;
  CellularError::FromDBusError(dberror, &error);
  callback->Run(error);
}

}  // namespace shill
