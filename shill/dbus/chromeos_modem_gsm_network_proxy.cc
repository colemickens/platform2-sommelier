// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/dbus/chromeos_modem_gsm_network_proxy.h"

#include <chromeos/dbus/service_constants.h>

#include "shill/cellular/cellular_error.h"
#include "shill/error.h"
#include "shill/logging.h"

using std::string;

namespace shill {

namespace Logging {
static auto kModuleLogScope = ScopeLogger::kDBus;
static string ObjectID(const dbus::ObjectPath* p) { return p->value(); }
}  // namespace Logging

// static.
const char ChromeosModemGsmNetworkProxy::kPropertyAccessTechnology[] =
    "AccessTechnology";

ChromeosModemGsmNetworkProxy::PropertySet::PropertySet(
    dbus::ObjectProxy* object_proxy,
    const std::string& interface_name,
    const PropertyChangedCallback& callback)
    : dbus::PropertySet(object_proxy, interface_name, callback) {
  RegisterProperty(kPropertyAccessTechnology, &access_technology);
}

ChromeosModemGsmNetworkProxy::ChromeosModemGsmNetworkProxy(
    const scoped_refptr<dbus::Bus>& bus,
    const string& path,
    const string& service)
    : proxy_(
        new org::freedesktop::ModemManager::Modem::Gsm::NetworkProxy(
            bus, service, dbus::ObjectPath(path))) {
  // Register signal handlers.
  proxy_->RegisterSignalQualitySignalHandler(
      base::Bind(&ChromeosModemGsmNetworkProxy::SignalQuality,
                 weak_factory_.GetWeakPtr()),
      base::Bind(&ChromeosModemGsmNetworkProxy::OnSignalConnected,
                 weak_factory_.GetWeakPtr()));
  proxy_->RegisterRegistrationInfoSignalHandler(
      base::Bind(&ChromeosModemGsmNetworkProxy::RegistrationInfo,
                 weak_factory_.GetWeakPtr()),
      base::Bind(&ChromeosModemGsmNetworkProxy::OnSignalConnected,
                 weak_factory_.GetWeakPtr()));
  proxy_->RegisterNetworkModeSignalHandler(
      base::Bind(&ChromeosModemGsmNetworkProxy::NetworkMode,
                 weak_factory_.GetWeakPtr()),
      base::Bind(&ChromeosModemGsmNetworkProxy::OnSignalConnected,
                 weak_factory_.GetWeakPtr()));

  // Register properties.
  properties_.reset(
      new PropertySet(
          proxy_->GetObjectProxy(),
          cromo::kModemGsmNetworkInterface,
          base::Bind(&ChromeosModemGsmNetworkProxy::OnPropertyChanged,
                     weak_factory_.GetWeakPtr())));

  // Connect property signals and initialize cached values. Based on
  // recommendations from src/dbus/property.h.
  properties_->ConnectSignals();
  properties_->GetAll();
}

ChromeosModemGsmNetworkProxy::~ChromeosModemGsmNetworkProxy() {}

void ChromeosModemGsmNetworkProxy::GetRegistrationInfo(
    Error* error,
    const RegistrationInfoCallback& callback,
    int timeout) {
  SLOG(&proxy_->GetObjectPath(), 2) << __func__;
  proxy_->GetRegistrationInfoAsync(
      base::Bind(&ChromeosModemGsmNetworkProxy::OnGetRegistrationInfoSuccess,
                 weak_factory_.GetWeakPtr(),
                 callback),
      base::Bind(&ChromeosModemGsmNetworkProxy::OnGetRegistrationInfoFailure,
                 weak_factory_.GetWeakPtr(),
                 callback),
      timeout);
}

void ChromeosModemGsmNetworkProxy::GetSignalQuality(
    Error* error,
    const SignalQualityCallback& callback,
    int timeout) {
  SLOG(&proxy_->GetObjectPath(), 2) << __func__;
  proxy_->GetSignalQualityAsync(
      base::Bind(&ChromeosModemGsmNetworkProxy::OnGetSignalQualitySuccess,
                 weak_factory_.GetWeakPtr(),
                 callback),
      base::Bind(&ChromeosModemGsmNetworkProxy::OnGetSignalQualityFailure,
                 weak_factory_.GetWeakPtr(),
                 callback),
      timeout);
}

void ChromeosModemGsmNetworkProxy::Register(const string& network_id,
                                            Error* error,
                                            const ResultCallback& callback,
                                            int timeout) {
  SLOG(&proxy_->GetObjectPath(), 2) << __func__ << ": " << network_id;
  proxy_->RegisterAsync(
      network_id,
      base::Bind(&ChromeosModemGsmNetworkProxy::OnRegisterSuccess,
                 weak_factory_.GetWeakPtr(),
                 callback),
      base::Bind(&ChromeosModemGsmNetworkProxy::OnRegisterFailure,
                 weak_factory_.GetWeakPtr(),
                 callback),
      timeout);
}

void ChromeosModemGsmNetworkProxy::Scan(Error* error,
                                        const ScanResultsCallback& callback,
                                        int timeout) {
  SLOG(&proxy_->GetObjectPath(), 2) << __func__;
  proxy_->ScanAsync(
      base::Bind(&ChromeosModemGsmNetworkProxy::OnScanSuccess,
                 weak_factory_.GetWeakPtr(),
                 callback),
      base::Bind(&ChromeosModemGsmNetworkProxy::OnScanFailure,
                 weak_factory_.GetWeakPtr(),
                 callback),
      timeout);
}

uint32_t ChromeosModemGsmNetworkProxy::AccessTechnology() {
  SLOG(&proxy_->GetObjectPath(), 2) << __func__;
  if (!properties_->access_technology.GetAndBlock()) {
    LOG(ERROR) << "Failed to get AccessTechnology";
    return 0;
  }
  return properties_->access_technology.value();
}

void ChromeosModemGsmNetworkProxy::SignalQuality(uint32_t quality) {
  SLOG(&proxy_->GetObjectPath(), 2) << __func__ << "(" << quality << ")";
  if (signal_quality_callback_.is_null()) {
    return;
  }
  signal_quality_callback_.Run(quality);
}

void ChromeosModemGsmNetworkProxy::RegistrationInfo(
    uint32_t status, const string& operator_code, const string& operator_name) {
  SLOG(&proxy_->GetObjectPath(), 2) << __func__ << "(" << status << ", "
      << operator_code << ", " << operator_name << ")";
  if (registration_info_callback_.is_null()) {
    return;
  }
  registration_info_callback_.Run(status, operator_code, operator_name);
}

void ChromeosModemGsmNetworkProxy::NetworkMode(uint32_t mode) {
  SLOG(&proxy_->GetObjectPath(), 2) << __func__ << "(" << mode << ")";
  if (network_mode_callback_.is_null()) {
    return;
  }
  network_mode_callback_.Run(mode);
}

void ChromeosModemGsmNetworkProxy::OnRegisterSuccess(
    const ResultCallback& callback) {
  SLOG(&proxy_->GetObjectPath(), 2) << __func__;
  callback.Run(Error());
}

void ChromeosModemGsmNetworkProxy::OnRegisterFailure(
    const ResultCallback& callback, brillo::Error* dbus_error) {
  SLOG(&proxy_->GetObjectPath(), 2) << __func__;
  Error error;
  CellularError::FromChromeosDBusError(dbus_error, &error);
  callback.Run(error);
}

void ChromeosModemGsmNetworkProxy::OnGetRegistrationInfoSuccess(
    const RegistrationInfoCallback& callback,
    const GsmRegistrationInfo& info) {
  SLOG(&proxy_->GetObjectPath(), 2) << __func__;
  callback.Run(
      std::get<0>(info), std::get<1>(info), std::get<2>(info), Error());
}

void ChromeosModemGsmNetworkProxy::OnGetRegistrationInfoFailure(
    const RegistrationInfoCallback& callback,
    brillo::Error* dbus_error) {
  SLOG(&proxy_->GetObjectPath(), 2) << __func__;
  Error error;
  CellularError::FromChromeosDBusError(dbus_error, &error);
  callback.Run(0, "", "", error);
}

void ChromeosModemGsmNetworkProxy::OnGetSignalQualitySuccess(
    const SignalQualityCallback& callback, uint32_t quality) {
  SLOG(&proxy_->GetObjectPath(), 2) << __func__ << "(" << quality << ")";
  callback.Run(quality, Error());
}

void ChromeosModemGsmNetworkProxy::OnGetSignalQualityFailure(
    const SignalQualityCallback& callback, brillo::Error* dbus_error) {
  SLOG(&proxy_->GetObjectPath(), 2) << __func__;
  Error error;
  CellularError::FromChromeosDBusError(dbus_error, &error);
  callback.Run(0, error);
}

void ChromeosModemGsmNetworkProxy::OnScanSuccess(
    const ScanResultsCallback& callback, const GsmScanResults& results) {
  SLOG(&proxy_->GetObjectPath(), 2) << __func__;
  callback.Run(results, Error());
}

void ChromeosModemGsmNetworkProxy::OnScanFailure(
    const ScanResultsCallback& callback, brillo::Error* dbus_error) {
  SLOG(&proxy_->GetObjectPath(), 2) << __func__;
  Error error;
  CellularError::FromChromeosDBusError(dbus_error, &error);
  callback.Run(GsmScanResults(), error);
}

void ChromeosModemGsmNetworkProxy::OnSignalConnected(
    const string& interface_name, const string& signal_name, bool success) {
  SLOG(&proxy_->GetObjectPath(), 2) << __func__
      << "interface: " << interface_name
             << " signal: " << signal_name << "success: " << success;
  if (!success) {
    LOG(ERROR) << "Failed to connect signal " << signal_name
        << " to interface " << interface_name;
  }
}

void ChromeosModemGsmNetworkProxy::OnPropertyChanged(
    const string& property_name) {
  SLOG(&proxy_->GetObjectPath(), 2) << __func__ << ": " << property_name;
}

}  // namespace shill
