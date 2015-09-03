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

#include "shill/cellular/mm1_modem_time_proxy.h"

#include <memory>

#include "shill/cellular/cellular_error.h"
#include "shill/dbus_async_call_helper.h"
#include "shill/logging.h"

using std::string;
using std::unique_ptr;

namespace shill {

namespace mm1 {

ModemTimeProxy::ModemTimeProxy(DBus::Connection* connection,
                               const string& path,
                               const string& service)
    : proxy_(connection, path, service) {}

ModemTimeProxy::~ModemTimeProxy() {}

void ModemTimeProxy::set_network_time_changed_callback(
    const NetworkTimeChangedSignalCallback& callback) {
  proxy_.set_network_time_changed_callback(callback);
}

void ModemTimeProxy::GetNetworkTime(Error* error,
                                    const StringCallback& callback,
                                    int timeout) {
  BeginAsyncDBusCall(__func__, proxy_, &Proxy::GetNetworkTimeAsync, callback,
                     error,  &CellularError::FromMM1DBusError, timeout);
}

ModemTimeProxy::Proxy::Proxy(DBus::Connection* connection,
                             const string& path,
                             const string& service)
    : DBus::ObjectProxy(*connection, path, service.c_str()) {}

ModemTimeProxy::Proxy::~Proxy() {}

void ModemTimeProxy::Proxy::set_network_time_changed_callback(
        const NetworkTimeChangedSignalCallback& callback) {
  network_time_changed_callback_ = callback;
}

// Signal callbacks inherited from Proxy
void ModemTimeProxy::Proxy::NetworkTimeChanged(const string& time) {
  SLOG(&path(), 2) << __func__;
  if (!network_time_changed_callback_.is_null())
    network_time_changed_callback_.Run(time);
}

// Method callbacks inherited from
// org::freedesktop::ModemManager1::Modem::TimeProxy
void ModemTimeProxy::Proxy::GetNetworkTimeCallback(const string& time,
                                                   const ::DBus::Error& dberror,
                                                   void* data) {
  SLOG(&path(), 2) << __func__;
  unique_ptr<StringCallback> callback(reinterpret_cast<StringCallback*>(data));
  Error error;
  CellularError::FromMM1DBusError(dberror, &error);
  callback->Run(time, error);
}

}  // namespace mm1
}  // namespace shill
