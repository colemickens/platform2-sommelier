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

#include "shill/cellular/modem_cdma_proxy.h"

#include <memory>

#include "shill/cellular/cellular_error.h"
#include "shill/dbus_async_call_helper.h"
#include "shill/logging.h"

using std::string;
using std::unique_ptr;

namespace shill {

ModemCDMAProxy::ModemCDMAProxy(
                               DBus::Connection* connection,
                               const string& path,
                               const string& service)
    : proxy_(connection, path, service) {}

ModemCDMAProxy::~ModemCDMAProxy() {}

void ModemCDMAProxy::Activate(const string& carrier, Error* error,
                              const ActivationResultCallback& callback,
                              int timeout) {
  BeginAsyncDBusCall(__func__, proxy_, &Proxy::ActivateAsync, callback,
                     error, &CellularError::FromDBusError, timeout,
                     carrier);
}

void ModemCDMAProxy::GetRegistrationState(
    Error* error,
    const RegistrationStateCallback& callback,
    int timeout) {
  BeginAsyncDBusCall(__func__, proxy_, &Proxy::GetRegistrationStateAsync,
                     callback, error, &CellularError::FromDBusError, timeout);
}

void ModemCDMAProxy::GetSignalQuality(Error* error,
                                      const SignalQualityCallback& callback,
                                      int timeout) {
  BeginAsyncDBusCall(__func__, proxy_, &Proxy::GetSignalQualityAsync, callback,
                     error, &CellularError::FromDBusError, timeout);
}

const string ModemCDMAProxy::MEID() {
  LOG(INFO) << "ModemCDMAProxy::" << __func__;
  SLOG(&proxy_.path(), 2) << __func__;
  try {
    return proxy_.Meid();
  } catch (const DBus::Error& e) {
    LOG(FATAL) << "DBus exception: " << e.name() << ": " << e.what();
    return string();  // Make the compiler happy.
  }
}

void ModemCDMAProxy::set_activation_state_callback(
    const ActivationStateSignalCallback& callback) {
  proxy_.set_activation_state_callback(callback);
}

void ModemCDMAProxy::set_signal_quality_callback(
    const SignalQualitySignalCallback& callback) {
  proxy_.set_signal_quality_callback(callback);
}

void ModemCDMAProxy::set_registration_state_callback(
    const RegistrationStateSignalCallback& callback) {
  proxy_.set_registration_state_callback(callback);
}

ModemCDMAProxy::Proxy::Proxy(DBus::Connection* connection,
                             const string& path,
                             const string& service)
    : DBus::ObjectProxy(*connection, path, service.c_str()) {}

ModemCDMAProxy::Proxy::~Proxy() {}

void ModemCDMAProxy::Proxy::set_activation_state_callback(
    const ActivationStateSignalCallback& callback) {
  activation_state_callback_ = callback;
}

void ModemCDMAProxy::Proxy::set_signal_quality_callback(
    const SignalQualitySignalCallback& callback) {
  signal_quality_callback_ = callback;
}

void ModemCDMAProxy::Proxy::set_registration_state_callback(
    const RegistrationStateSignalCallback& callback) {
  registration_state_callback_ = callback;
}

void ModemCDMAProxy::Proxy::ActivationStateChanged(
    const uint32_t& activation_state,
    const uint32_t& activation_error,
    const DBusPropertiesMap& status_changes) {
  SLOG(&path(), 2) << __func__ << "(" << activation_state << ", "
                   << activation_error << ")";
  activation_state_callback_.Run(activation_state,
                                  activation_error,
                                  status_changes);
}

void ModemCDMAProxy::Proxy::SignalQuality(const uint32_t& quality) {
  SLOG(&path(), 2) << __func__ << "(" << quality << ")";
  signal_quality_callback_.Run(quality);
}

void ModemCDMAProxy::Proxy::RegistrationStateChanged(
    const uint32_t& cdma_1x_state,
    const uint32_t& evdo_state) {
  SLOG(&path(), 2) << __func__ << "(" << cdma_1x_state << ", "
                   << evdo_state << ")";
  registration_state_callback_.Run(cdma_1x_state, evdo_state);
}

void ModemCDMAProxy::Proxy::ActivateCallback(const uint32_t& status,
                                             const DBus::Error& dberror,
                                             void* data) {
  SLOG(&path(), 2) << __func__ << "(" << status << ")";
  unique_ptr<ActivationResultCallback> callback(
      reinterpret_cast<ActivationResultCallback*>(data));
  Error error;
  CellularError::FromDBusError(dberror, &error);
  callback->Run(status, error);
}

void ModemCDMAProxy::Proxy::GetRegistrationStateCallback(
    const uint32_t& state_1x, const uint32_t& state_evdo,
    const DBus::Error& dberror, void* data) {
  SLOG(&path(), 2) << __func__ << "(" << state_1x << ", " << state_evdo << ")";
  unique_ptr<RegistrationStateCallback> callback(
      reinterpret_cast<RegistrationStateCallback*>(data));
  Error error;
  CellularError::FromDBusError(dberror, &error);
  callback->Run(state_1x, state_evdo, error);
}


void ModemCDMAProxy::Proxy::GetSignalQualityCallback(const uint32_t& quality,
                                                     const DBus::Error& dberror,
                                                     void* data) {
  SLOG(&path(), 2) << __func__ << "(" << quality << ")";
  unique_ptr<SignalQualityCallback> callback(
      reinterpret_cast<SignalQualityCallback*>(data));
  Error error;
  CellularError::FromDBusError(dberror, &error);
  callback->Run(quality, error);
}

}  // namespace shill
