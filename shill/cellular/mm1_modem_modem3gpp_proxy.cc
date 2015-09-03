//
// Copyright (C) 2012 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#include "shill/cellular/mm1_modem_modem3gpp_proxy.h"

#include <memory>

#include "shill/cellular/cellular_error.h"
#include "shill/dbus_async_call_helper.h"
#include "shill/logging.h"

using std::string;
using std::unique_ptr;

namespace shill {

namespace mm1 {

ModemModem3gppProxy::ModemModem3gppProxy(
    DBus::Connection* connection,
    const string& path,
    const string& service)
    : proxy_(connection, path, service) {}

ModemModem3gppProxy::~ModemModem3gppProxy() {}

void ModemModem3gppProxy::Register(const std::string& operator_id,
                                   Error* error,
                                   const ResultCallback& callback,
                                   int timeout) {
  BeginAsyncDBusCall(__func__, proxy_, &Proxy::RegisterAsync, callback,
                     error, &CellularError::FromMM1DBusError, timeout,
                     operator_id);
}

void ModemModem3gppProxy::Scan(Error* error,
                               const DBusPropertyMapsCallback& callback,
                               int timeout) {
  BeginAsyncDBusCall(__func__, proxy_, &Proxy::ScanAsync, callback,
                     error, &CellularError::FromMM1DBusError, timeout);
}

// ModemModem3gppProxy::Proxy
ModemModem3gppProxy::Proxy::Proxy(DBus::Connection* connection,
                                  const std::string& path,
                                  const std::string& service)
    : DBus::ObjectProxy(*connection, path, service.c_str()) {}

ModemModem3gppProxy::Proxy::~Proxy() {}

// Method callbacks inherited from
// org::freedesktop::ModemManager1::Modem::ModemModem3gppProxy
void ModemModem3gppProxy::Proxy::RegisterCallback(const ::DBus::Error& dberror,
                                                  void* data) {
  SLOG(&path(), 2) << __func__;
  unique_ptr<ResultCallback> callback(reinterpret_cast<ResultCallback*>(data));
  Error error;
  CellularError::FromMM1DBusError(dberror, &error);
  callback->Run(error);
}

void ModemModem3gppProxy::Proxy::ScanCallback(
    const std::vector<DBusPropertiesMap>& results,
    const ::DBus::Error& dberror, void* data) {
  SLOG(&path(), 2) << __func__;
  unique_ptr<DBusPropertyMapsCallback> callback(
      reinterpret_cast<DBusPropertyMapsCallback*>(data));
  Error error;
  CellularError::FromMM1DBusError(dberror, &error);
  callback->Run(results, error);
}

}  // namespace mm1
}  // namespace shill
