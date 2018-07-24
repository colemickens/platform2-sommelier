// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/dbus/chromeos_modem_gobi_proxy.h"

#include <memory>

#include <base/bind.h>

#include "shill/cellular/cellular_error.h"
#include "shill/error.h"
#include "shill/logging.h"

using std::string;

namespace shill {

namespace Logging {
static auto kModuleLogScope = ScopeLogger::kDBus;
static string ObjectID(const dbus::ObjectPath* p) { return p->value(); }
}  // namespace Logging

ChromeosModemGobiProxy::ChromeosModemGobiProxy(
    const scoped_refptr<dbus::Bus>& bus,
    const string& path,
    const string& service)
    : proxy_(
        new org::chromium::ModemManager::Modem::GobiProxy(
            bus, service, dbus::ObjectPath(path))) {}

ChromeosModemGobiProxy::~ChromeosModemGobiProxy() {}

void ChromeosModemGobiProxy::SetCarrier(const string& carrier,
                                        Error* error,
                                        const ResultCallback& callback,
                                        int timeout) {
  SLOG(&proxy_->GetObjectPath(), 2) << __func__ << ": " << carrier;
  proxy_->SetCarrierAsync(
      carrier,
      base::Bind(&ChromeosModemGobiProxy::OnSetCarrierSuccess,
                 weak_factory_.GetWeakPtr(),
                 callback),
      base::Bind(&ChromeosModemGobiProxy::OnSetCarrierFailure,
                 weak_factory_.GetWeakPtr(),
                 callback));
}

void ChromeosModemGobiProxy::OnSetCarrierSuccess(
    const ResultCallback& callback) {
  SLOG(&proxy_->GetObjectPath(), 2) << __func__;
  callback.Run(Error());
}

void ChromeosModemGobiProxy::OnSetCarrierFailure(
    const ResultCallback& callback, chromeos::Error* dbus_error) {
  SLOG(&proxy_->GetObjectPath(), 2) << __func__;
  Error error;
  CellularError::FromChromeosDBusError(dbus_error, &error);
  callback.Run(error);
}

}  // namespace shill
