// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/dbus/chromeos_mm1_modem_proxy.h"

#include <tuple>

#include "shill/cellular/cellular_error.h"
#include "shill/logging.h"

using std::string;

namespace shill {

namespace Logging {
static auto kModuleLogScope = ScopeLogger::kDBus;
static string ObjectID(const dbus::ObjectPath* p) { return p->value(); }
}  // namespace Logging

namespace mm1 {

ChromeosModemProxy::ChromeosModemProxy(const scoped_refptr<dbus::Bus>& bus,
                                       const RpcIdentifier& path,
                                       const string& service)
    : proxy_(
        new org::freedesktop::ModemManager1::ModemProxy(
            bus, service, path)) {
  // Register signal handlers.
  proxy_->RegisterStateChangedSignalHandler(
      base::Bind(&ChromeosModemProxy::StateChanged,
                 weak_factory_.GetWeakPtr()),
      base::Bind(&ChromeosModemProxy::OnSignalConnected,
                 weak_factory_.GetWeakPtr()));
}

ChromeosModemProxy::~ChromeosModemProxy() = default;

void ChromeosModemProxy::Enable(bool enable,
                                Error* error,
                                const ResultCallback& callback,
                                int timeout) {
  SLOG(&proxy_->GetObjectPath(), 2) << __func__ << ": " << enable;
  proxy_->EnableAsync(enable,
                      base::Bind(&ChromeosModemProxy::OnOperationSuccess,
                                 weak_factory_.GetWeakPtr(),
                                 callback,
                                 __func__),
                      base::Bind(&ChromeosModemProxy::OnOperationFailure,
                                 weak_factory_.GetWeakPtr(),
                                 callback,
                                 __func__),
                      timeout);
}

void ChromeosModemProxy::CreateBearer(
    const KeyValueStore& properties,
    Error* error,
    const RpcIdentifierCallback& callback,
    int timeout) {
  SLOG(&proxy_->GetObjectPath(), 2) << __func__;
  brillo::VariantDictionary properties_dict =
      KeyValueStore::ConvertToVariantDictionary(properties);
  proxy_->CreateBearerAsync(
      properties_dict,
      base::Bind(&ChromeosModemProxy::OnCreateBearerSuccess,
                 weak_factory_.GetWeakPtr(),
                 callback),
      base::Bind(&ChromeosModemProxy::OnCreateBearerFailure,
                 weak_factory_.GetWeakPtr(),
                 callback),
      timeout);
}

void ChromeosModemProxy::DeleteBearer(const RpcIdentifier& bearer,
                                      Error* error,
                                      const ResultCallback& callback,
                                      int timeout) {
  SLOG(&proxy_->GetObjectPath(), 2) << __func__ << ": " << bearer.value();
  proxy_->DeleteBearerAsync(bearer,
                            base::Bind(&ChromeosModemProxy::OnOperationSuccess,
                                       weak_factory_.GetWeakPtr(),
                                       callback,
                                       __func__),
                            base::Bind(&ChromeosModemProxy::OnOperationFailure,
                                       weak_factory_.GetWeakPtr(),
                                       callback,
                                       __func__),
                            timeout);
}

void ChromeosModemProxy::Reset(Error* error,
                               const ResultCallback& callback,
                               int timeout) {
  SLOG(&proxy_->GetObjectPath(), 2) << __func__;
  proxy_->ResetAsync(base::Bind(&ChromeosModemProxy::OnOperationSuccess,
                                weak_factory_.GetWeakPtr(),
                                callback,
                                __func__),
                     base::Bind(&ChromeosModemProxy::OnOperationFailure,
                                weak_factory_.GetWeakPtr(),
                                callback,
                                __func__),
                     timeout);
}

void ChromeosModemProxy::FactoryReset(const std::string& code,
                                      Error* error,
                                      const ResultCallback& callback,
                                      int timeout) {
  SLOG(&proxy_->GetObjectPath(), 2) << __func__;
  proxy_->FactoryResetAsync(code,
                            base::Bind(&ChromeosModemProxy::OnOperationSuccess,
                                       weak_factory_.GetWeakPtr(),
                                       callback,
                                       __func__),
                            base::Bind(&ChromeosModemProxy::OnOperationFailure,
                                       weak_factory_.GetWeakPtr(),
                                       callback,
                                       __func__),
                            timeout);
}

void ChromeosModemProxy::SetCurrentCapabilities(uint32_t capabilities,
                                                Error* error,
                                                const ResultCallback& callback,
                                                int timeout) {
  SLOG(&proxy_->GetObjectPath(), 2) << __func__ << ": " << capabilities;
  proxy_->SetCurrentCapabilitiesAsync(
      capabilities,
      base::Bind(&ChromeosModemProxy::OnOperationSuccess,
                 weak_factory_.GetWeakPtr(),
                 callback,
                 __func__),
      base::Bind(&ChromeosModemProxy::OnOperationFailure,
                 weak_factory_.GetWeakPtr(),
                 callback,
                 __func__),
      timeout);
}

void ChromeosModemProxy::SetCurrentModes(uint32_t allowed_modes,
                                         uint32_t preferred_mode,
                                         Error* error,
                                         const ResultCallback& callback,
                                         int timeout) {
  SLOG(&proxy_->GetObjectPath(), 2) << __func__ << ": " << allowed_modes
                                    << " " << preferred_mode;
  std::tuple<uint32_t, uint32_t> modes { allowed_modes, preferred_mode };
  proxy_->SetCurrentModesAsync(
      modes,
      base::Bind(&ChromeosModemProxy::OnOperationSuccess,
                 weak_factory_.GetWeakPtr(),
                 callback,
                 __func__),
      base::Bind(&ChromeosModemProxy::OnOperationFailure,
                 weak_factory_.GetWeakPtr(),
                 callback,
                 __func__),
      timeout);
}

void ChromeosModemProxy::SetCurrentBands(const std::vector<uint32_t>& bands,
                                         Error* error,
                                         const ResultCallback& callback,
                                         int timeout) {
  SLOG(&proxy_->GetObjectPath(), 2) << __func__;
  proxy_->SetCurrentBandsAsync(
      bands,
      base::Bind(&ChromeosModemProxy::OnOperationSuccess,
                 weak_factory_.GetWeakPtr(),
                 callback,
                 __func__),
      base::Bind(&ChromeosModemProxy::OnOperationFailure,
                 weak_factory_.GetWeakPtr(),
                 callback,
                 __func__),
      timeout);
}

void ChromeosModemProxy::Command(const std::string& cmd,
                                 uint32_t user_timeout,
                                 Error* error,
                                 const StringCallback& callback,
                                 int timeout) {
  SLOG(&proxy_->GetObjectPath(), 2) << __func__ << ": " << cmd;
  proxy_->CommandAsync(cmd,
                       user_timeout,
                       base::Bind(&ChromeosModemProxy::OnCommandSuccess,
                                  weak_factory_.GetWeakPtr(),
                                  callback),
                       base::Bind(&ChromeosModemProxy::OnCommandFailure,
                                  weak_factory_.GetWeakPtr(),
                                  callback),
                       timeout);
}

void ChromeosModemProxy::SetPowerState(uint32_t power_state,
                                       Error* error,
                                       const ResultCallback& callback,
                                       int timeout) {
  SLOG(&proxy_->GetObjectPath(), 2) << __func__ << ": " << power_state;
  proxy_->SetPowerStateAsync(
      power_state,
      base::Bind(&ChromeosModemProxy::OnOperationSuccess,
                 weak_factory_.GetWeakPtr(),
                 callback,
                 __func__),
      base::Bind(&ChromeosModemProxy::OnOperationFailure,
                 weak_factory_.GetWeakPtr(),
                 callback,
                 __func__),
      timeout);
}

void ChromeosModemProxy::StateChanged(
    int32_t old, int32_t _new, uint32_t reason) {
  SLOG(&proxy_->GetObjectPath(), 2) << __func__;
  if (state_changed_callback_.is_null()) {
    return;
  }
  state_changed_callback_.Run(old, _new, reason);
}

void ChromeosModemProxy::OnCreateBearerSuccess(
    const RpcIdentifierCallback& callback, const dbus::ObjectPath& path) {
  SLOG(&proxy_->GetObjectPath(), 2) << __func__ << ": " << path.value();
  callback.Run(path, Error());
}

void ChromeosModemProxy::OnCreateBearerFailure(
    const RpcIdentifierCallback& callback, brillo::Error* dbus_error) {
  SLOG(&proxy_->GetObjectPath(), 2) << __func__;
  Error error;
  CellularError::FromMM1ChromeosDBusError(dbus_error, &error);
  callback.Run(RpcIdentifier(""), error);
}

void ChromeosModemProxy::OnCommandSuccess(const StringCallback& callback,
                                          const string& response) {
  SLOG(&proxy_->GetObjectPath(), 2) << __func__ << ": " << response;
  callback.Run(response, Error());
}

void ChromeosModemProxy::OnCommandFailure(const StringCallback& callback,
                                          brillo::Error* dbus_error) {
  SLOG(&proxy_->GetObjectPath(), 2) << __func__;
  Error error;
  CellularError::FromMM1ChromeosDBusError(dbus_error, &error);
  callback.Run("", error);
}

void ChromeosModemProxy::OnOperationSuccess(const ResultCallback& callback,
                                            const string& operation) {
  SLOG(&proxy_->GetObjectPath(), 2) << __func__ << ": " << operation;
  callback.Run(Error());
}

void ChromeosModemProxy::OnOperationFailure(const ResultCallback& callback,
                                            const string& operation,
                                            brillo::Error* dbus_error) {
  SLOG(&proxy_->GetObjectPath(), 2) << __func__ << ": " << operation;
  Error error;
  CellularError::FromMM1ChromeosDBusError(dbus_error, &error);
  callback.Run(error);
}

void ChromeosModemProxy::OnSignalConnected(
    const string& interface_name, const string& signal_name, bool success) {
  SLOG(&proxy_->GetObjectPath(), 2) << __func__
      << "interface: " << interface_name
             << " signal: " << signal_name << "success: " << success;
  if (!success) {
    LOG(ERROR) << "Failed to connect signal " << signal_name
        << " to interface " << interface_name;
  }
}

}  // namespace mm1
}  // namespace shill
