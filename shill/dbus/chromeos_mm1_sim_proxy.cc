// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/dbus/chromeos_mm1_sim_proxy.h"

#include "shill/cellular/cellular_error.h"
#include "shill/logging.h"

using std::string;

namespace shill {

namespace Logging {
static auto kModuleLogScope = ScopeLogger::kDBus;
static string ObjectID(const dbus::ObjectPath* p) { return p->value(); }
}  // namespace Logging

namespace mm1 {

ChromeosSimProxy::ChromeosSimProxy(const scoped_refptr<dbus::Bus>& bus,
                                   const RpcIdentifier& path,
                                   const string& service)
    : proxy_(
        new org::freedesktop::ModemManager1::SimProxy(
            bus, service, dbus::ObjectPath(path))) {}

ChromeosSimProxy::~ChromeosSimProxy() = default;

void ChromeosSimProxy::SendPin(const string& pin,
                               Error* error,
                               const ResultCallback& callback,
                               int timeout) {
  // pin is intentionally not logged.
  SLOG(&proxy_->GetObjectPath(), 2) << __func__;
  proxy_->SendPinAsync(pin,
                       base::Bind(&ChromeosSimProxy::OnOperationSuccess,
                                  weak_factory_.GetWeakPtr(),
                                  callback,
                                  __func__),
                       base::Bind(&ChromeosSimProxy::OnOperationFailure,
                                  weak_factory_.GetWeakPtr(),
                                  callback,
                                  __func__),
                       timeout);
}

void ChromeosSimProxy::SendPuk(const string& puk,
                               const string& pin,
                               Error* error,
                               const ResultCallback& callback,
                               int timeout) {
  // pin and puk are intentionally not logged.
  SLOG(&proxy_->GetObjectPath(), 2) << __func__;
  proxy_->SendPukAsync(puk,
                       pin,
                       base::Bind(&ChromeosSimProxy::OnOperationSuccess,
                                  weak_factory_.GetWeakPtr(),
                                  callback,
                                  __func__),
                       base::Bind(&ChromeosSimProxy::OnOperationFailure,
                                  weak_factory_.GetWeakPtr(),
                                  callback,
                                  __func__),
                       timeout);
}

void ChromeosSimProxy::EnablePin(const string& pin,
                                 const bool enabled,
                                 Error* error,
                                 const ResultCallback& callback,
                                 int timeout) {
  // pin is intentionally not logged.
  SLOG(&proxy_->GetObjectPath(), 2) << __func__ << ": " << enabled;
  proxy_->EnablePinAsync(pin,
                         enabled,
                         base::Bind(&ChromeosSimProxy::OnOperationSuccess,
                                    weak_factory_.GetWeakPtr(),
                                    callback,
                                    __func__),
                         base::Bind(&ChromeosSimProxy::OnOperationFailure,
                                    weak_factory_.GetWeakPtr(),
                                    callback,
                                    __func__),
                         timeout);
}

void ChromeosSimProxy::ChangePin(const string& old_pin,
                                 const string& new_pin,
                                 Error* error,
                                 const ResultCallback& callback,
                                 int timeout) {
  // old_pin and new_pin are intentionally not logged.
  SLOG(&proxy_->GetObjectPath(), 2) << __func__;
  proxy_->ChangePinAsync(old_pin,
                      new_pin,
                      base::Bind(&ChromeosSimProxy::OnOperationSuccess,
                                 weak_factory_.GetWeakPtr(),
                                 callback,
                                 __func__),
                      base::Bind(&ChromeosSimProxy::OnOperationFailure,
                                 weak_factory_.GetWeakPtr(),
                                 callback,
                                 __func__),
                      timeout);
}

void ChromeosSimProxy::OnOperationSuccess(const ResultCallback& callback,
                                          const string& operation) {
  SLOG(&proxy_->GetObjectPath(), 2) << __func__ << ": " << operation;
  callback.Run(Error());
}

void ChromeosSimProxy::OnOperationFailure(const ResultCallback& callback,
                                          const string& operation,
                                          brillo::Error* dbus_error) {
  SLOG(&proxy_->GetObjectPath(), 2) << __func__ << ": " << operation;
  Error error;
  CellularError::FromMM1ChromeosDBusError(dbus_error, &error);
  callback.Run(error);
}

}  // namespace mm1
}  // namespace shill
