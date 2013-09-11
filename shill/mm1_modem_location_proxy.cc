// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mm1_modem_location_proxy.h"

#include "shill/cellular_error.h"
#include "shill/logging.h"

using std::string;

namespace shill {
namespace mm1 {

ModemLocationProxy::ModemLocationProxy(DBus::Connection *connection,
                                       const string &path,
                                       const string &service)
    : proxy_(connection, path, service) {}

ModemLocationProxy::~ModemLocationProxy() {}

void ModemLocationProxy::Setup(uint32_t sources,
                               bool signal_location,
                               Error *error,
                               const ResultCallback &callback,
                               int timeout) {
  SLOG(Modem, 2) << __func__;
  scoped_ptr<ResultCallback> cb(new ResultCallback(callback));
  try {
    SLOG(DBus, 2) << __func__;
    proxy_.Setup(sources, signal_location, cb.get(), timeout);
    cb.release();
  } catch (const DBus::Error &e) {
    if (error)
      CellularError::FromMM1DBusError(e, error);
  }
}

void ModemLocationProxy::GetLocation(Error *error,
                                     const DBusEnumValueMapCallback &callback,
                                     int timeout) {
  SLOG(Modem, 2) << __func__;
  scoped_ptr<DBusEnumValueMapCallback> cb(
      new DBusEnumValueMapCallback(callback));
  try {
    SLOG(DBus, 2) << __func__;
    proxy_.GetLocation(cb.get(), timeout);
    cb.release();
  } catch (const DBus::Error &e) {
    if (error)
      CellularError::FromMM1DBusError(e, error);
  }
}

ModemLocationProxy::Proxy::Proxy(DBus::Connection *connection,
                                 const string &path,
                                 const string &service)
    : DBus::ObjectProxy(*connection, path, service.c_str()) {}

ModemLocationProxy::Proxy::~Proxy() {}

// Method callbacks inherited from
// org::freedesktop::ModemManager1::Modem:LocationProxy
void ModemLocationProxy::Proxy::SetupCallback(const ::DBus::Error &dberror,
                                              void *data) {
  SLOG(DBus, 2) << __func__;
  scoped_ptr<ResultCallback> callback(reinterpret_cast<ResultCallback *>(data));
  Error error;
  CellularError::FromMM1DBusError(dberror, &error);
  callback->Run(error);
}

void ModemLocationProxy::Proxy::GetLocationCallback(
    const DBusEnumValueMap &location,
    const ::DBus::Error &dberror,
    void *data) {
  SLOG(DBus, 2) << __func__;
  scoped_ptr<DBusEnumValueMapCallback> callback(
      reinterpret_cast<DBusEnumValueMapCallback *>(data));
  Error error;
  CellularError::FromMM1DBusError(dberror, &error);
  callback->Run(location, error);
}

}  // namespace mm1
}  // namespace shill
