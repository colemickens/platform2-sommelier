// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/mm1_modem_modem3gpp_proxy.h"

#include "shill/cellular_error.h"
#include "shill/logging.h"

using std::string;

namespace shill {
namespace mm1 {

ModemModem3gppProxy::ModemModem3gppProxy(
    DBus::Connection *connection,
    const string &path,
    const string &service)
    : proxy_(connection, path, service) {}

ModemModem3gppProxy::~ModemModem3gppProxy() {}

void ModemModem3gppProxy::Register(const std::string &operator_id,
                                   Error *error,
                                   const ResultCallback &callback,
                                   int timeout) {
  scoped_ptr<ResultCallback> cb(new ResultCallback(callback));
  try {
    SLOG(DBus, 2) << __func__;
    proxy_.Register(operator_id, cb.get(), timeout);
    cb.release();
  } catch (const DBus::Error &e) {
    if (error)
      CellularError::FromDBusError(e, error);
  }
}

void ModemModem3gppProxy::Scan(Error *error,
                               const DBusPropertyMapsCallback &callback,
                               int timeout) {
  scoped_ptr<DBusPropertyMapsCallback> cb(
      new DBusPropertyMapsCallback(callback));
  try {
    SLOG(DBus, 2) << __func__;
    proxy_.Scan(cb.get(), timeout);
    cb.release();
  } catch (const DBus::Error &e) {
    if (error)
      CellularError::FromDBusError(e, error);
  }
}

// Inherited properties from ModemModem3gppProxyInterface.
std::string ModemModem3gppProxy::Imei() {
  SLOG(DBus, 2) << __func__;
  try {
    return proxy_.Imei();
  } catch (const DBus::Error &e) {
    LOG(FATAL) << "DBus exception: " << e.name() << ": " << e.what();
    return std::string();  // Make the compiler happy.
  }
};
uint32_t ModemModem3gppProxy::RegistrationState() {
  SLOG(DBus, 2) << __func__;
  try {
    return proxy_.RegistrationState();
  } catch (const DBus::Error &e) {
    LOG(FATAL) << "DBus exception: " << e.name() << ": " << e.what();
    return 0;  // Make the compiler happy.
  }
};
std::string ModemModem3gppProxy::OperatorCode() {
  SLOG(DBus, 2) << __func__;
  try {
    return proxy_.OperatorCode();
  } catch (const DBus::Error &e) {
    LOG(FATAL) << "DBus exception: " << e.name() << ": " << e.what();
    return std::string();  // Make the compiler happy.
  }
};
std::string ModemModem3gppProxy::OperatorName() {
  SLOG(DBus, 2) << __func__;
  try {
    return proxy_.OperatorName();
  } catch (const DBus::Error &e) {
    LOG(FATAL) << "DBus exception: " << e.name() << ": " << e.what();
    return std::string();  // Make the compiler happy.
  }
};
uint32_t ModemModem3gppProxy::EnabledFacilityLocks() {
  SLOG(DBus, 2) << __func__;
  try {
    return proxy_.EnabledFacilityLocks();
  } catch (const DBus::Error &e) {
    LOG(FATAL) << "DBus exception: " << e.name() << ": " << e.what();
    return 0;  // Make the compiler happy.
  }
};

// ModemModem3gppProxy::Proxy
ModemModem3gppProxy::Proxy::Proxy(DBus::Connection *connection,
                                  const std::string &path,
                                  const std::string &service)
    : DBus::ObjectProxy(*connection, path, service.c_str()) {}

ModemModem3gppProxy::Proxy::~Proxy() {}

// Method callbacks inherited from
// org::freedesktop::ModemManager1::Modem::ModemModem3gppProxy
void ModemModem3gppProxy::Proxy::RegisterCallback(const ::DBus::Error& dberror,
                                                  void *data) {
  SLOG(DBus, 2) << __func__;
  scoped_ptr<ResultCallback> callback(reinterpret_cast<ResultCallback *>(data));
  Error error;
  CellularError::FromDBusError(dberror, &error);
  callback->Run(error);
}

void ModemModem3gppProxy::Proxy::ScanCallback(
    const std::vector<DBusPropertiesMap> &results,
    const ::DBus::Error& dberror, void *data) {
  SLOG(DBus, 2) << __func__;
  scoped_ptr<DBusPropertyMapsCallback> callback(
      reinterpret_cast<DBusPropertyMapsCallback *>(data));
  Error error;
  CellularError::FromDBusError(dberror, &error);
  callback->Run(results, error);
}

}  // namespace mm1
}  // namespace shill
