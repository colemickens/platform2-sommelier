// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/mm1_sim_proxy.h"

#include <base/logging.h>

#include "shill/cellular_error.h"
#include "shill/scope_logger.h"

using std::string;

namespace shill {
namespace mm1 {

SimProxy::SimProxy(DBus::Connection *connection,
                   const string &path,
                   const string &service)
    : proxy_(connection, path, service) {}

SimProxy::~SimProxy() {}


void SimProxy::SendPin(const string &pin,
                       Error *error,
                       const ResultCallback &callback,
                       int timeout) {
  // pin is intentionally not logged.
  SLOG(Modem, 2) << __func__ << "( XXX, " << timeout << ")";
  scoped_ptr<ResultCallback> cb(new ResultCallback(callback));
  try {
    SLOG(DBus, 2) << __func__;
    proxy_.SendPin(pin, cb.get(), timeout);
    cb.release();
  } catch (const DBus::Error &e) {
    if (error)
      CellularError::FromDBusError(e, error);
  }
}

void SimProxy::SendPuk(const string &puk,
                       const string &pin,
                       Error *error,
                       const ResultCallback &callback,
                       int timeout) {
  // pin and puk are intentionally not logged.
  SLOG(Modem, 2) << __func__ << "( XXX, XXX, " << timeout << ")";
  scoped_ptr<ResultCallback> cb(new ResultCallback(callback));
  try {
    SLOG(DBus, 2) << __func__;
    proxy_.SendPuk(puk, pin, cb.get(), timeout);
    cb.release();
  } catch (const DBus::Error &e) {
    if (error)
      CellularError::FromDBusError(e, error);
  }
}

void SimProxy::EnablePin(const string &pin,
                         const bool enabled,
                         Error *error,
                         const ResultCallback &callback,
                         int timeout) {
  // pin is intentionally not logged.
  SLOG(Modem, 2) << __func__ << "( XXX, " << enabled << ", " << timeout << ")";
  scoped_ptr<ResultCallback> cb(new ResultCallback(callback));
  try {
    SLOG(DBus, 2) << __func__;
    proxy_.EnablePin(pin, enabled, cb.get(), timeout);
    cb.release();
  } catch (const DBus::Error &e) {
    if (error)
      CellularError::FromDBusError(e, error);
  }
}

void SimProxy::ChangePin(const string &old_pin,
                         const string &new_pin,
                         Error *error,
                         const ResultCallback &callback,
                         int timeout) {
  // old_pin and new_pin are intentionally not logged.
  SLOG(Modem, 2) << __func__ << "( XXX, XXX, " << timeout << ")";
  scoped_ptr<ResultCallback> cb(new ResultCallback(callback));
  try {
    SLOG(DBus, 2) << __func__;
    proxy_.ChangePin(old_pin, new_pin, cb.get(), timeout);
    cb.release();
  } catch (const DBus::Error &e) {
    if (error)
      CellularError::FromDBusError(e, error);
  }
}

// Inherited properties from SimProxyInterface.
const string SimProxy::SimIdentifier() {
  SLOG(DBus, 2) << __func__;
  try {
    return proxy_.SimIdentifier();
  } catch (const DBus::Error &e) {
    LOG(FATAL) << "DBus exception: " << e.name() << ": " << e.what();
    return string();  // Make the compiler happy.
  }
}

const string SimProxy::Imsi() {
  SLOG(DBus, 2) << __func__;
  try {
    return proxy_.Imsi();
  } catch (const DBus::Error &e) {
    LOG(FATAL) << "DBus exception: " << e.name() << ": " << e.what();
    return string();  // Make the compiler happy.
  }
}

const string SimProxy::OperatorIdentifier() {
  SLOG(DBus, 2) << __func__;
  try {
    return proxy_.OperatorIdentifier();
  } catch (const DBus::Error &e) {
    LOG(FATAL) << "DBus exception: " << e.name() << ": " << e.what();
    return string();  // Make the compiler happy.
  }
}

const string SimProxy::OperatorName() {
  SLOG(DBus, 2) << __func__;
  try {
    return proxy_.OperatorName();
  } catch (const DBus::Error &e) {
    LOG(FATAL) << "DBus exception: " << e.name() << ": " << e.what();
    return string();  // Make the compiler happy.
  }
}

SimProxy::Proxy::Proxy(DBus::Connection *connection,
                       const string &path,
                       const string &service)
    : DBus::ObjectProxy(*connection, path, service.c_str()) {}

SimProxy::Proxy::~Proxy() {}


// Method callbacks inherited from
// org::freedesktop::ModemManager1::SimProxy
void SimProxy::Proxy::SendPinCallback(const ::DBus::Error &dberror,
                                      void *data) {
  SLOG(DBus, 2) << __func__;
  scoped_ptr<ResultCallback> callback(reinterpret_cast<ResultCallback *>(data));
  Error error;
  CellularError::FromDBusError(dberror, &error);
  callback->Run(error);
}

void SimProxy::Proxy::SendPukCallback(const ::DBus::Error &dberror,
                                      void *data)  {
  SLOG(DBus, 2) << __func__;
  scoped_ptr<ResultCallback> callback(reinterpret_cast<ResultCallback *>(data));
  Error error;
  CellularError::FromDBusError(dberror, &error);
  callback->Run(error);
}

void SimProxy::Proxy::EnablePinCallback(const ::DBus::Error &dberror,
                                        void *data)  {
  SLOG(DBus, 2) << __func__;
  scoped_ptr<ResultCallback> callback(reinterpret_cast<ResultCallback *>(data));
  Error error;
  CellularError::FromDBusError(dberror, &error);
  callback->Run(error);
}

void SimProxy::Proxy::ChangePinCallback(const ::DBus::Error &dberror,
                                        void *data) {
  SLOG(DBus, 2) << __func__;
  scoped_ptr<ResultCallback> callback(reinterpret_cast<ResultCallback *>(data));
  Error error;
  CellularError::FromDBusError(dberror, &error);
  callback->Run(error);
}

}  // namespace mm1
}  // namespace shill
