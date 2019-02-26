// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/dbus/chromeos_mm1_modem_simple_proxy.h"

#include "shill/cellular/cellular_error.h"
#include "shill/logging.h"

using std::string;

namespace shill {

namespace Logging {
static auto kModuleLogScope = ScopeLogger::kDBus;
static string ObjectID(const dbus::ObjectPath* p) { return p->value(); }
}  // namespace Logging

namespace mm1 {

ChromeosModemSimpleProxy::ChromeosModemSimpleProxy(
    const scoped_refptr<dbus::Bus>& bus,
    const string& path,
    const string& service)
    : proxy_(
        new org::freedesktop::ModemManager1::Modem::SimpleProxy(
            bus, service, dbus::ObjectPath(path))) {}

ChromeosModemSimpleProxy::~ChromeosModemSimpleProxy() {}

void ChromeosModemSimpleProxy::Connect(
    const KeyValueStore& properties,
    Error* error,
    const RpcIdentifierCallback& callback,
    int timeout) {
  SLOG(&proxy_->GetObjectPath(), 2) << __func__;
  brillo::VariantDictionary properties_dict =
      KeyValueStore::ConvertToVariantDictionary(properties);
  proxy_->ConnectAsync(properties_dict,
                       base::Bind(&ChromeosModemSimpleProxy::OnConnectSuccess,
                                  weak_factory_.GetWeakPtr(),
                                  callback),
                       base::Bind(&ChromeosModemSimpleProxy::OnConnectFailure,
                                  weak_factory_.GetWeakPtr(),
                                  callback),
                       timeout);
}

void ChromeosModemSimpleProxy::Disconnect(const string& bearer,
                                          Error* error,
                                          const ResultCallback& callback,
                                          int timeout) {
  SLOG(&proxy_->GetObjectPath(), 2) << __func__ << ": " << bearer;
  proxy_->DisconnectAsync(
      dbus::ObjectPath(bearer),
      base::Bind(&ChromeosModemSimpleProxy::OnDisconnectSuccess,
                 weak_factory_.GetWeakPtr(),
                 callback),
      base::Bind(&ChromeosModemSimpleProxy::OnDisconnectFailure,
                 weak_factory_.GetWeakPtr(),
                 callback),
      timeout);
}

void ChromeosModemSimpleProxy::GetStatus(Error* error,
                                         const KeyValueStoreCallback& callback,
                                         int timeout) {
  SLOG(&proxy_->GetObjectPath(), 2) << __func__;
  proxy_->GetStatusAsync(
      base::Bind(&ChromeosModemSimpleProxy::OnGetStatusSuccess,
                 weak_factory_.GetWeakPtr(),
                 callback),
      base::Bind(&ChromeosModemSimpleProxy::OnGetStatusFailure,
                 weak_factory_.GetWeakPtr(),
                 callback),
      timeout);
}

void ChromeosModemSimpleProxy::OnConnectSuccess(
    const RpcIdentifierCallback& callback, const dbus::ObjectPath& path) {
  SLOG(&proxy_->GetObjectPath(), 2) << __func__ << ": " << path.value();
  callback.Run(path.value(), Error());
}

void ChromeosModemSimpleProxy::OnConnectFailure(
    const RpcIdentifierCallback& callback, brillo::Error* dbus_error) {
  SLOG(&proxy_->GetObjectPath(), 2) << __func__;
  Error error;
  CellularError::FromMM1ChromeosDBusError(dbus_error, &error);
  callback.Run("", error);
}

void ChromeosModemSimpleProxy::OnDisconnectSuccess(
    const ResultCallback& callback) {
  SLOG(&proxy_->GetObjectPath(), 2) << __func__;
  callback.Run(Error());
}

void ChromeosModemSimpleProxy::OnDisconnectFailure(
    const ResultCallback& callback, brillo::Error* dbus_error) {
  SLOG(&proxy_->GetObjectPath(), 2) << __func__;
  Error error;
  CellularError::FromMM1ChromeosDBusError(dbus_error, &error);
  callback.Run(error);
}

void ChromeosModemSimpleProxy::OnGetStatusSuccess(
    const KeyValueStoreCallback& callback,
    const brillo::VariantDictionary& status) {
  SLOG(&proxy_->GetObjectPath(), 2) << __func__;
  KeyValueStore status_store =
      KeyValueStore::ConvertFromVariantDictionary(status);
  callback.Run(status_store, Error());
}

void ChromeosModemSimpleProxy::OnGetStatusFailure(
    const KeyValueStoreCallback& callback, brillo::Error* dbus_error) {
  SLOG(&proxy_->GetObjectPath(), 2) << __func__;
  Error error;
  CellularError::FromMM1ChromeosDBusError(dbus_error, &error);
  callback.Run(KeyValueStore(), error);
}

}  // namespace mm1
}  // namespace shill
