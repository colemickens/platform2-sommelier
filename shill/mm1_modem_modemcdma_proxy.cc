// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/mm1_modem_modemcdma_proxy.h"

#include <base/logging.h>

#include "cellular_error.h"

using std::string;

namespace shill {
namespace mm1 {

ModemModemCdmaProxy::ModemModemCdmaProxy(ModemModemCdmaProxyDelegate *delegate,
                                         DBus::Connection *connection,
                                         const string &path,
                                         const string &service)
    : proxy_(delegate, connection, path, service) {}

ModemModemCdmaProxy::~ModemModemCdmaProxy() {}

void ModemModemCdmaProxy::Activate(const std::string &carrier,
                                   AsyncCallHandler *call_handler,
                                   int timeout) {
  proxy_.Activate(carrier, call_handler, timeout);
}

void ModemModemCdmaProxy::ActivateManual(
    const DBusPropertiesMap &properties,
    AsyncCallHandler *call_handler,
    int timeout) {
  proxy_.ActivateManual(properties, call_handler, timeout);
}

// Inherited properties from ModemModemCdmaProxyInterface.
std::string ModemModemCdmaProxy::Meid() {
  return proxy_.Meid();
};
std::string ModemModemCdmaProxy::Esn() {
  return proxy_.Esn();
};
uint32_t ModemModemCdmaProxy::Sid() {
  return proxy_.Sid();
};
uint32_t ModemModemCdmaProxy::Nid() {
  return proxy_.Nid();
};
uint32_t ModemModemCdmaProxy::Cdma1xRegistrationState() {
  return proxy_.Cdma1xRegistrationState();
};
uint32_t ModemModemCdmaProxy::EvdoRegistrationState() {
  return proxy_.EvdoRegistrationState();
};

// ModemModemCdmaProxy::Proxy
ModemModemCdmaProxy::Proxy::Proxy(ModemModemCdmaProxyDelegate *delegate,
                                  DBus::Connection *connection,
                                  const std::string &path,
                                  const std::string &service)
    : DBus::ObjectProxy(*connection, path, service.c_str()),
      delegate_(delegate) {}

ModemModemCdmaProxy::Proxy::~Proxy() {}

// Signal callbacks inherited from Proxy
void ModemModemCdmaProxy::Proxy::ActivationStateChanged(
    const uint32_t &activation_state,
    const uint32_t &activation_error,
    const DBusPropertiesMap &status_changes) {
  delegate_->OnActivationStateChanged(activation_state,
                                      activation_error,
                                      status_changes);
}

// Method callbacks inherited from
// org::freedesktop::ModemManager1::Modem::ModemModemCdmaProxy
void ModemModemCdmaProxy::Proxy::ActivateCallback(const ::DBus::Error& dberror,
                                                  void *data) {
  AsyncCallHandler *call_handler = reinterpret_cast<AsyncCallHandler *>(data);
  Error error;
  CellularError::FromDBusError(dberror, &error),
      delegate_->OnActivateCallback(error, call_handler);
}

void ModemModemCdmaProxy::Proxy::ActivateManualCallback(
    const ::DBus::Error& dberror,
    void *data) {
  AsyncCallHandler *call_handler = reinterpret_cast<AsyncCallHandler *>(data);
  Error error;
  CellularError::FromDBusError(dberror, &error),
      delegate_->OnActivateManualCallback(error, call_handler);
}

}  // namespace mm1
}  // namespace shill
