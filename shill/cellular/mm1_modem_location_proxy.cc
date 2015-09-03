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

#include "shill/cellular/mm1_modem_location_proxy.h"

#include <memory>

#include "shill/cellular/cellular_error.h"
#include "shill/dbus_async_call_helper.h"
#include "shill/logging.h"

using std::string;
using std::unique_ptr;

namespace shill {

namespace mm1 {

ModemLocationProxy::ModemLocationProxy(DBus::Connection* connection,
                                       const string& path,
                                       const string& service)
    : proxy_(connection, path, service) {}

ModemLocationProxy::~ModemLocationProxy() {}

void ModemLocationProxy::Setup(uint32_t sources,
                               bool signal_location,
                               Error* error,
                               const ResultCallback& callback,
                               int timeout) {
  BeginAsyncDBusCall(__func__, proxy_, &Proxy::SetupAsync, callback,
                     error, &CellularError::FromMM1DBusError, timeout,
                     sources, signal_location);
}

void ModemLocationProxy::GetLocation(Error* error,
                                     const DBusEnumValueMapCallback& callback,
                                     int timeout) {
  BeginAsyncDBusCall(__func__, proxy_, &Proxy::GetLocationAsync, callback,
                     error, &CellularError::FromMM1DBusError, timeout);
}

ModemLocationProxy::Proxy::Proxy(DBus::Connection* connection,
                                 const string& path,
                                 const string& service)
    : DBus::ObjectProxy(*connection, path, service.c_str()) {}

ModemLocationProxy::Proxy::~Proxy() {}

// Method callbacks inherited from
// org::freedesktop::ModemManager1::Modem:LocationProxy
void ModemLocationProxy::Proxy::SetupCallback(const ::DBus::Error& dberror,
                                              void* data) {
  SLOG(&path(), 2) << __func__;
  unique_ptr<ResultCallback> callback(reinterpret_cast<ResultCallback*>(data));
  Error error;
  CellularError::FromMM1DBusError(dberror, &error);
  callback->Run(error);
}

void ModemLocationProxy::Proxy::GetLocationCallback(
    const DBusEnumValueMap& location,
    const ::DBus::Error& dberror,
    void* data) {
  SLOG(&path(), 2) << __func__;
  unique_ptr<DBusEnumValueMapCallback> callback(
      reinterpret_cast<DBusEnumValueMapCallback*>(data));
  Error error;
  CellularError::FromMM1DBusError(dberror, &error);
  callback->Run(location, error);
}

}  // namespace mm1
}  // namespace shill
