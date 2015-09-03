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

#include "shill/cellular/modem_gobi_proxy.h"

#include <memory>

#include <base/bind.h>

#include "shill/cellular/cellular_error.h"
#include "shill/dbus_async_call_helper.h"
#include "shill/error.h"
#include "shill/logging.h"

using base::Bind;
using base::Callback;
using std::string;
using std::unique_ptr;

namespace shill {

ModemGobiProxy::ModemGobiProxy(DBus::Connection* connection,
                               const string& path,
                               const string& service)
    : proxy_(connection, path, service) {}

ModemGobiProxy::~ModemGobiProxy() {}

void ModemGobiProxy::SetCarrier(const string& carrier,
                                Error* error,
                                const ResultCallback& callback,
                                int timeout) {
  BeginAsyncDBusCall(__func__, proxy_, &Proxy::SetCarrierAsync, callback,
                     error, &CellularError::FromDBusError, timeout,
                     carrier);
}

ModemGobiProxy::Proxy::Proxy(DBus::Connection* connection,
                             const string& path,
                             const string& service)
    : DBus::ObjectProxy(*connection, path, service.c_str()) {}

ModemGobiProxy::Proxy::~Proxy() {}

void ModemGobiProxy::Proxy::SetCarrierCallback(const DBus::Error& dberror,
                                               void* data) {
  SLOG(&path(), 2) << __func__;
  unique_ptr<ResultCallback> callback(reinterpret_cast<ResultCallback*>(data));
  Error error;
  CellularError::FromDBusError(dberror, &error);
  callback->Run(error);
}

}  // namespace shill
