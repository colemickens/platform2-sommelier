// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/dbus/chromeos_mm1_modem_location_proxy.h"

#include <memory>

#include "shill/cellular/cellular_error.h"
#include "shill/logging.h"

using std::string;

namespace shill {

namespace Logging {
static auto kModuleLogScope = ScopeLogger::kDBus;
static string ObjectID(const dbus::ObjectPath* p) { return p->value(); }
}  // namespace Logging

namespace mm1 {

ChromeosModemLocationProxy::ChromeosModemLocationProxy(
    const scoped_refptr<dbus::Bus>& bus,
    const string& path,
    const string& service)
    : proxy_(
        new org::freedesktop::ModemManager1::Modem::LocationProxy(
            bus, service, dbus::ObjectPath(path))) {
}

ChromeosModemLocationProxy::~ChromeosModemLocationProxy() = default;

void ChromeosModemLocationProxy::Setup(uint32_t sources, bool signal_location,
                                       Error* error,
                                       const ResultCallback& callback,
                                       int timeout) {
  SLOG(&proxy_->GetObjectPath(), 2) << __func__ << ": " << sources << ", "
                                    << signal_location;
  proxy_->SetupAsync(sources, signal_location,
                     base::Bind(&ChromeosModemLocationProxy::OnSetupSuccess,
                                weak_factory_.GetWeakPtr(), callback),
                     base::Bind(&ChromeosModemLocationProxy::OnSetupFailure,
                                weak_factory_.GetWeakPtr(), callback),
                     timeout);
}

void ChromeosModemLocationProxy::GetLocation(Error* error,
                                             const BrilloAnyCallback& callback,
                                             int timeout) {
  SLOG(&proxy_->GetObjectPath(), 2) << __func__;
  proxy_->GetLocationAsync(
      base::Bind(&ChromeosModemLocationProxy::OnGetLocationSuccess,
                 weak_factory_.GetWeakPtr(), callback),
      base::Bind(&ChromeosModemLocationProxy::OnGetLocationFailure,
                 weak_factory_.GetWeakPtr(), callback),
      timeout);
}

void ChromeosModemLocationProxy::OnSetupSuccess(
    const ResultCallback& callback) {
  SLOG(&proxy_->GetObjectPath(), 2) << __func__;
  callback.Run(Error());
}

void ChromeosModemLocationProxy::OnSetupFailure(
    const ResultCallback& callback, brillo::Error* dbus_error) {
  SLOG(&proxy_->GetObjectPath(), 2) << __func__;
  Error error;
  CellularError::FromMM1ChromeosDBusError(dbus_error, &error);
  callback.Run(error);
}

void ChromeosModemLocationProxy::OnGetLocationSuccess(
    const BrilloAnyCallback& callback,
    const std::map<uint32_t, brillo::Any>& results) {
  SLOG(&proxy_->GetObjectPath(), 2) << __func__;
  callback.Run(results, Error());
}

void ChromeosModemLocationProxy::OnGetLocationFailure(
    const BrilloAnyCallback& callback, brillo::Error* dbus_error) {
  SLOG(&proxy_->GetObjectPath(), 2) << __func__;
  Error error;
  CellularError::FromMM1ChromeosDBusError(dbus_error, &error);
  callback.Run(std::map<uint32_t, brillo::Any>(), error);
}

}  // namespace mm1
}  // namespace shill
