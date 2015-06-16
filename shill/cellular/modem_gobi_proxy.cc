// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
