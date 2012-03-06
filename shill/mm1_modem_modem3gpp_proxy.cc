// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/mm1_modem_modem3gpp_proxy.h"

#include <base/logging.h>

#include "cellular_error.h"

using std::string;

namespace shill {
namespace mm1 {

ModemModem3gppProxy::ModemModem3gppProxy(
    ModemModem3gppProxyDelegate *delegate,
    DBus::Connection *connection,
    const string &path,
    const string &service)
    : proxy_(delegate, connection, path, service) {}

ModemModem3gppProxy::~ModemModem3gppProxy() {}

void ModemModem3gppProxy::Register(const std::string &operator_id,
                                   AsyncCallHandler *call_handler,
                                   int timeout) {
  proxy_.Register(operator_id, call_handler, timeout);
}

void ModemModem3gppProxy::Scan(AsyncCallHandler *call_handler, int timeout) {
  proxy_.Scan(call_handler, timeout);
}

// Inherited properties from ModemModem3gppProxyInterface.
std::string ModemModem3gppProxy::Imei() {
  return proxy_.Imei();
};
uint32_t ModemModem3gppProxy::RegistrationState() {
  return proxy_.RegistrationState();
};
std::string ModemModem3gppProxy::OperatorCode() {
  return proxy_.OperatorCode();
};
std::string ModemModem3gppProxy::OperatorName() {
  return proxy_.OperatorName();
};
uint32_t ModemModem3gppProxy::EnabledFacilityLocks() {
  return proxy_.EnabledFacilityLocks();
};

// ModemModem3gppProxy::Proxy
ModemModem3gppProxy::Proxy::Proxy(ModemModem3gppProxyDelegate *delegate,
                                  DBus::Connection *connection,
                                  const std::string &path,
                                  const std::string &service)
    : DBus::ObjectProxy(*connection, path, service.c_str()),
      delegate_(delegate) {}

ModemModem3gppProxy::Proxy::~Proxy() {}

// Method callbacks inherited from
// org::freedesktop::ModemManager1::Modem::ModemModem3gppProxy
void ModemModem3gppProxy::Proxy::RegisterCallback(const ::DBus::Error& dberror,
                                                  void *data) {
  AsyncCallHandler *call_handler = reinterpret_cast<AsyncCallHandler *>(data);
  Error error;
  CellularError::FromDBusError(dberror, &error),
      delegate_->OnRegisterCallback(error, call_handler);
}

void ModemModem3gppProxy::Proxy::ScanCallback(
    const std::vector<DBusPropertiesMap> &results,
    const ::DBus::Error& dberror, void *data) {
  AsyncCallHandler *call_handler = reinterpret_cast<AsyncCallHandler *>(data);
  Error error;
  CellularError::FromDBusError(dberror, &error),
      delegate_->OnScanCallback(results, error, call_handler);
}

}  // namespace mm1
}  // namespace shill
