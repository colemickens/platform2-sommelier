// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/mm1_modem_simple_proxy.h"

#include <base/logging.h>

#include "shill/cellular_error.h"
#include "shill/scope_logger.h"

using std::string;

namespace shill {
namespace mm1 {

ModemSimpleProxy::ModemSimpleProxy(DBus::Connection *connection,
                                   const string &path,
                                   const string &service)
    : proxy_(connection, path, service) {}

ModemSimpleProxy::~ModemSimpleProxy() {}

void ModemSimpleProxy::Connect(
    const DBusPropertiesMap &properties,
    Error *error,
    const DBusPathCallback &callback,
    int timeout) {
  scoped_ptr<DBusPathCallback> cb(new DBusPathCallback(callback));
  try {
    SLOG(DBus, 2) << __func__;
    proxy_.Connect(properties, cb.get(), timeout);
    cb.release();
  } catch (const DBus::Error &e) {
    if (error)
      CellularError::FromDBusError(e, error);
  }
}

void ModemSimpleProxy::Disconnect(const ::DBus::Path &bearer,
                                  Error *error,
                                  const ResultCallback &callback,
                                  int timeout) {
  scoped_ptr<ResultCallback> cb(new ResultCallback(callback));
  try {
    SLOG(DBus, 2) << __func__;
    proxy_.Disconnect(bearer, cb.get(), timeout);
    cb.release();
  } catch (const DBus::Error &e) {
    if (error)
      CellularError::FromDBusError(e, error);
  }
}

void ModemSimpleProxy::GetStatus(Error *error,
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


// ModemSimpleProxy::Proxy
ModemSimpleProxy::Proxy::Proxy(DBus::Connection *connection,
                               const std::string &path,
                               const std::string &service)
    : DBus::ObjectProxy(*connection, path, service.c_str()) {}

ModemSimpleProxy::Proxy::~Proxy() {}

// Method callbacks inherited from
// org::freedesktop::ModemManager1::Modem::ModemSimpleProxy
void ModemSimpleProxy::Proxy::ConnectCallback(const ::DBus::Path &bearer,
                                              const ::DBus::Error &dberror,
                                              void *data) {
  SLOG(DBus, 2) << __func__;
  scoped_ptr<DBusPathCallback> callback(
      reinterpret_cast<DBusPathCallback *>(data));
  Error error;
  CellularError::FromDBusError(dberror, &error);
  callback->Run(bearer, error);
}

void ModemSimpleProxy::Proxy::DisconnectCallback(const ::DBus::Error &dberror,
                                                 void *data) {
  SLOG(DBus, 2) << __func__;
  scoped_ptr<ResultCallback> callback(reinterpret_cast<ResultCallback *>(data));
  Error error;
  CellularError::FromDBusError(dberror, &error);
  callback->Run(error);
}

void ModemSimpleProxy::Proxy::GetStatusCallback(
    const DBusPropertiesMap &properties,
    const ::DBus::Error &dberror,
    void *data) {
  SLOG(DBus, 2) << __func__;
  scoped_ptr<DBusPropertyMapCallback> callback(
      reinterpret_cast<DBusPropertyMapCallback *>(data));
  Error error;
  CellularError::FromDBusError(dberror, &error);
  callback->Run(properties, error);
}

}  // namespace mm1
}  // namespace shill
