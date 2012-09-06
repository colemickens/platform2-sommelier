// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/modem_gobi_proxy.h"

#include <base/bind.h>

#include "shill/cellular_error.h"
#include "shill/error.h"
#include "shill/logging.h"

using base::Bind;
using base::Callback;
using std::string;

namespace shill {

ModemGobiProxy::ModemGobiProxy(DBus::Connection *connection,
                               const string &path,
                               const string &service)
    : proxy_(connection, path, service) {}

ModemGobiProxy::~ModemGobiProxy() {}

void ModemGobiProxy::SetCarrier(const string &carrier,
                                Error *error,
                                const ResultCallback &callback,
                                int timeout) {
  scoped_ptr<ResultCallback> cb(new ResultCallback(callback));
  try {
    SLOG(DBus, 2) << __func__;
    proxy_.SetCarrier(carrier, cb.get(), timeout);
    cb.release();
  } catch (const DBus::Error &e) {
    if (error) {
      CellularError::FromDBusError(e, error);
    }
  }
}

ModemGobiProxy::Proxy::Proxy(DBus::Connection *connection,
                             const string &path,
                             const string &service)
    : DBus::ObjectProxy(*connection, path, service.c_str()) {}

ModemGobiProxy::Proxy::~Proxy() {}

void ModemGobiProxy::Proxy::SetCarrierCallback(const DBus::Error &dberror,
                                               void *data) {
  SLOG(DBus, 2) << __func__;
  scoped_ptr<ResultCallback> callback(reinterpret_cast<ResultCallback *>(data));
  Error error;
  CellularError::FromDBusError(dberror, &error);
  callback->Run(error);
}

}  // namespace shill
