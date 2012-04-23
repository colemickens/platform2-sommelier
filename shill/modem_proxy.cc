// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/modem_proxy.h"

#include <base/bind.h>
#include <base/logging.h>

#include "shill/cellular_error.h"
#include "shill/error.h"
#include "shill/scope_logger.h"

using base::Bind;
using base::Callback;
using std::string;

namespace shill {

typedef Callback<void(const ModemHardwareInfo &,
                      const Error &)> ModemInfoCallback;

ModemProxy::ModemProxy(DBus::Connection *connection,
                       const string &path,
                       const string &service)
    : proxy_(connection, path, service) {}

ModemProxy::~ModemProxy() {}

void ModemProxy::set_state_changed_callback(
      const ModemStateChangedSignalCallback &callback) {
  proxy_.set_state_changed_callback(callback);
}

void ModemProxy::Enable(bool enable, Error *error,
                        const ResultCallback &callback, int timeout) {
  SLOG(Modem, 2) << __func__ << "(" << enable << ", " << timeout << ")";
  scoped_ptr<ResultCallback> cb(new ResultCallback(callback));
  try {
    SLOG(DBus, 2) << __func__;
    proxy_.Enable(enable, cb.get(), timeout);
    cb.release();
  } catch (DBus::Error e) {
    if (error)
      CellularError::FromDBusError(e, error);
  }
}

void ModemProxy::Disconnect(Error *error, const ResultCallback &callback,
                            int timeout) {
  scoped_ptr<ResultCallback> cb(new ResultCallback(callback));
  try {
    SLOG(DBus, 2) << __func__;
    proxy_.Disconnect(cb.get(), timeout);
    cb.release();
  } catch (DBus::Error e) {
    if (error)
      CellularError::FromDBusError(e, error);
  }
}

void ModemProxy::GetModemInfo(Error *error,
                              const ModemInfoCallback &callback,
                              int timeout) {
  scoped_ptr<ModemInfoCallback> cb(new ModemInfoCallback(callback));
  try {
    SLOG(DBus, 2) << __func__;
    proxy_.GetInfo(cb.get(), timeout);
    cb.release();
  } catch (DBus::Error e) {
    if (error)
      CellularError::FromDBusError(e, error);
  }
}

ModemProxy::Proxy::Proxy(DBus::Connection *connection,
                         const string &path,
                         const string &service)
    : DBus::ObjectProxy(*connection, path, service.c_str()) {}

ModemProxy::Proxy::~Proxy() {}

void ModemProxy::Proxy::set_state_changed_callback(
      const ModemStateChangedSignalCallback &callback) {
  state_changed_callback_ = callback;
}

void ModemProxy::Proxy::StateChanged(
    const uint32 &old, const uint32 &_new, const uint32 &reason) {
  SLOG(DBus, 2) << __func__ << "(" << old << ", " << _new << ", "
                 << reason << ")";
  state_changed_callback_.Run(old, _new, reason);
}

void ModemProxy::Proxy::EnableCallback(const DBus::Error &dberror, void *data) {
  SLOG(DBus, 2) << __func__;
  scoped_ptr<ResultCallback> callback(reinterpret_cast<ResultCallback *>(data));
  Error error;
  CellularError::FromDBusError(dberror, &error);
  callback->Run(error);
}

void ModemProxy::Proxy::GetInfoCallback(const ModemHardwareInfo &info,
                                        const DBus::Error &dberror,
                                        void *data) {
  SLOG(DBus, 2) << __func__;
  scoped_ptr<ModemInfoCallback> callback(
      reinterpret_cast<ModemInfoCallback *>(data));
  Error error;
  CellularError::FromDBusError(dberror, &error);
  callback->Run(info, error);
}

void ModemProxy::Proxy::DisconnectCallback(const DBus::Error &dberror,
                                           void *data) {
  SLOG(DBus, 2) << __func__;
  scoped_ptr<ResultCallback> callback(reinterpret_cast<ResultCallback *>(data));
  Error error;
  CellularError::FromDBusError(dberror, &error);
  callback->Run(error);
}

}  // namespace shill
