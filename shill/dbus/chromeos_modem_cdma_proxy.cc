// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/dbus/chromeos_modem_cdma_proxy.h"

#include <chromeos/dbus/service_constants.h>

#include "shill/cellular/cellular_error.h"
#include "shill/logging.h"

using std::string;

namespace shill {

namespace Logging {
static auto kModuleLogScope = ScopeLogger::kDBus;
static string ObjectID(const dbus::ObjectPath* p) { return p->value(); }
}  // namespace Logging

// static.
const char ChromeosModemCdmaProxy::kPropertyMeid[] = "Meid";

ChromeosModemCdmaProxy::PropertySet::PropertySet(
    dbus::ObjectProxy* object_proxy,
    const std::string& interface_name,
    const PropertyChangedCallback& callback)
    : dbus::PropertySet(object_proxy, interface_name, callback) {
  RegisterProperty(kPropertyMeid, &meid);
}

ChromeosModemCdmaProxy::ChromeosModemCdmaProxy(
    const scoped_refptr<dbus::Bus>& bus,
    const string& path,
    const string& service)
    : proxy_(
        new org::freedesktop::ModemManager::Modem::CdmaProxy(
            bus, service, dbus::ObjectPath(path))) {
  // Register signal handlers.
  proxy_->RegisterActivationStateChangedSignalHandler(
      base::Bind(&ChromeosModemCdmaProxy::ActivationStateChanged,
                 weak_factory_.GetWeakPtr()),
      base::Bind(&ChromeosModemCdmaProxy::OnSignalConnected,
                 weak_factory_.GetWeakPtr()));
  proxy_->RegisterSignalQualitySignalHandler(
      base::Bind(&ChromeosModemCdmaProxy::SignalQuality,
                 weak_factory_.GetWeakPtr()),
      base::Bind(&ChromeosModemCdmaProxy::OnSignalConnected,
                 weak_factory_.GetWeakPtr()));
  proxy_->RegisterRegistrationStateChangedSignalHandler(
      base::Bind(&ChromeosModemCdmaProxy::RegistrationStateChanged,
                 weak_factory_.GetWeakPtr()),
      base::Bind(&ChromeosModemCdmaProxy::OnSignalConnected,
                 weak_factory_.GetWeakPtr()));

  // Register properties.
  properties_.reset(
      new PropertySet(
          proxy_->GetObjectProxy(),
          cromo::kModemCdmaInterface,
          base::Bind(&ChromeosModemCdmaProxy::OnPropertyChanged,
                     weak_factory_.GetWeakPtr())));

  // Connect property signals and initialize cached value. Based on
  // recommendations from src/dbus/property.h.
  properties_->ConnectSignals();
  properties_->GetAll();
}

ChromeosModemCdmaProxy::~ChromeosModemCdmaProxy() {}

void ChromeosModemCdmaProxy::Activate(const string& carrier,
                                      Error* error,
                                      const ActivationResultCallback& callback,
                                      int timeout) {
  SLOG(&proxy_->GetObjectPath(), 2) << __func__ << ": " << carrier;
  proxy_->ActivateAsync(
      carrier,
      base::Bind(&ChromeosModemCdmaProxy::OnActivateSuccess,
                 weak_factory_.GetWeakPtr(),
                 callback),
      base::Bind(&ChromeosModemCdmaProxy::OnActivateFailure,
                 weak_factory_.GetWeakPtr(),
                 callback),
      timeout);
}

void ChromeosModemCdmaProxy::GetRegistrationState(
    Error* error,
    const RegistrationStateCallback& callback,
    int timeout) {
  SLOG(&proxy_->GetObjectPath(), 2) << __func__;
  proxy_->GetRegistrationStateAsync(
      base::Bind(&ChromeosModemCdmaProxy::OnGetRegistrationStateSuccess,
                 weak_factory_.GetWeakPtr(),
                 callback),
      base::Bind(&ChromeosModemCdmaProxy::OnGetRegistrationStateFailure,
                 weak_factory_.GetWeakPtr(),
                 callback),
      timeout);
}

void ChromeosModemCdmaProxy::GetSignalQuality(
    Error* error, const SignalQualityCallback& callback, int timeout) {
  SLOG(&proxy_->GetObjectPath(), 2) << __func__;
  proxy_->GetSignalQualityAsync(
      base::Bind(&ChromeosModemCdmaProxy::OnGetSignalQualitySuccess,
                 weak_factory_.GetWeakPtr(),
                 callback),
      base::Bind(&ChromeosModemCdmaProxy::OnGetSignalQualityFailure,
                 weak_factory_.GetWeakPtr(),
                 callback),
      timeout);
}

const string ChromeosModemCdmaProxy::MEID() {
  SLOG(&proxy_->GetObjectPath(), 2) << __func__;
  if (!properties_->meid.GetAndBlock()) {
    LOG(ERROR) << "Failed to get MEID";
    return string();
  }
  return properties_->meid.value();
}

void ChromeosModemCdmaProxy::ActivationStateChanged(
    uint32_t activation_state,
    uint32_t activation_error,
    const brillo::VariantDictionary& status_changes) {
  SLOG(&proxy_->GetObjectPath(), 2) << __func__ << "(" << activation_state
                                    << ", " << activation_error << ")";
  if (activation_state_callback_.is_null()) {
    return;
  }
  KeyValueStore status_changes_store =
      KeyValueStore::ConvertFromVariantDictionary(status_changes);
  activation_state_callback_.Run(activation_state,
                                 activation_error,
                                 status_changes_store);
}

void ChromeosModemCdmaProxy::SignalQuality(uint32_t quality) {
  SLOG(&proxy_->GetObjectPath(), 2) << __func__ << "(" << quality << ")";
  if (signal_quality_callback_.is_null()) {
    return;
  }
  signal_quality_callback_.Run(quality);
}

void ChromeosModemCdmaProxy::RegistrationStateChanged(
    uint32_t cdma_1x_state,
    uint32_t evdo_state) {
  SLOG(&proxy_->GetObjectPath(), 2) << __func__ << "(" << cdma_1x_state << ", "
                                    << evdo_state << ")";
  if (registration_state_callback_.is_null()) {
    return;
  }
  registration_state_callback_.Run(cdma_1x_state, evdo_state);
}

void ChromeosModemCdmaProxy::OnActivateSuccess(
    const ActivationResultCallback& callback, uint32_t status) {
  SLOG(&proxy_->GetObjectPath(), 2) << __func__ << "(" << status << ")";
  callback.Run(status, Error());
}

void ChromeosModemCdmaProxy::OnActivateFailure(
    const ActivationResultCallback& callback, brillo::Error* dbus_error) {
  SLOG(&proxy_->GetObjectPath(), 2) << __func__;
  Error error;
  CellularError::FromChromeosDBusError(dbus_error, &error);
  callback.Run(0, error);
}

void ChromeosModemCdmaProxy::OnGetRegistrationStateSuccess(
    const RegistrationStateCallback& callback,
    uint32_t state_1x,
    uint32_t state_evdo) {
  SLOG(&proxy_->GetObjectPath(), 2) << __func__ << "(" << state_1x
                                    << ", " << state_evdo << ")";
  callback.Run(state_1x, state_evdo, Error());
}

void ChromeosModemCdmaProxy::OnGetRegistrationStateFailure(
    const RegistrationStateCallback& callback, brillo::Error* dbus_error) {
  SLOG(&proxy_->GetObjectPath(), 2) << __func__;
  Error error;
  CellularError::FromChromeosDBusError(dbus_error, &error);
  callback.Run(0, 0, error);
}

void ChromeosModemCdmaProxy::OnGetSignalQualitySuccess(
    const SignalQualityCallback& callback, uint32_t quality) {
  SLOG(&proxy_->GetObjectPath(), 2) << __func__ << "(" << quality << ")";
  callback.Run(quality, Error());
}

void ChromeosModemCdmaProxy::OnGetSignalQualityFailure(
    const SignalQualityCallback& callback, brillo::Error* dbus_error) {
  SLOG(&proxy_->GetObjectPath(), 2) << __func__;
  Error error;
  CellularError::FromChromeosDBusError(dbus_error, &error);
  callback.Run(0, error);
}

void ChromeosModemCdmaProxy::OnSignalConnected(
    const string& interface_name, const string& signal_name, bool success) {
  SLOG(&proxy_->GetObjectPath(), 2) << __func__
      << "interface: " << interface_name
             << " signal: " << signal_name << "success: " << success;
  if (!success) {
    LOG(ERROR) << "Failed to connect signal " << signal_name
        << " to interface " << interface_name;
  }
}

void ChromeosModemCdmaProxy::OnPropertyChanged(
    const string& property_name) {
  SLOG(&proxy_->GetObjectPath(), 2) << __func__ << ": " << property_name;
}

}  // namespace shill
