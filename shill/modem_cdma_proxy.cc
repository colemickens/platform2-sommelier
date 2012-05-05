// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/modem_cdma_proxy.h"

#include <base/logging.h>

#include "shill/cellular_error.h"
#include "shill/scope_logger.h"

using std::string;

namespace shill {

ModemCDMAProxy::ModemCDMAProxy(
                               DBus::Connection *connection,
                               const string &path,
                               const string &service)
    : proxy_(connection, path, service) {}

ModemCDMAProxy::~ModemCDMAProxy() {}

void ModemCDMAProxy::Activate(const string &carrier, Error *error,
                              const ActivationResultCallback &callback,
                              int timeout) {
  scoped_ptr<ActivationResultCallback>
      cb(new ActivationResultCallback(callback));
  try {
    SLOG(DBus, 2) << __func__;
    proxy_.Activate(carrier, cb.get(), timeout);
    cb.release();
  } catch (const DBus::Error &e) {
    if (error)
      CellularError::FromDBusError(e, error);
  }
}

void ModemCDMAProxy::GetRegistrationState(
    Error *error,
    const RegistrationStateCallback &callback,
    int timeout) {
  scoped_ptr<RegistrationStateCallback>
      cb(new RegistrationStateCallback(callback));
  try {
    SLOG(DBus, 2) << __func__;
    proxy_.GetRegistrationState(cb.get(), timeout);
    cb.release();
  } catch (const DBus::Error &e) {
    if (error)
      CellularError::FromDBusError(e, error);
  }
}

void ModemCDMAProxy::GetSignalQuality(Error *error,
                                      const SignalQualityCallback &callback,
                                      int timeout) {
  scoped_ptr<SignalQualityCallback> cb(new SignalQualityCallback(callback));
  try {
    SLOG(DBus, 2) << __func__;
    proxy_.GetSignalQuality(cb.get(), timeout);
    cb.release();
  } catch (const DBus::Error &e) {
    if (error)
      CellularError::FromDBusError(e, error);
  }
}

const string ModemCDMAProxy::MEID() {
  LOG(INFO) << "ModemCDMAProxy::" << __func__;
  SLOG(DBus, 2) << __func__;
  return proxy_.Meid();
}

void ModemCDMAProxy::set_activation_state_callback(
    const ActivationStateSignalCallback &callback) {
  proxy_.set_activation_state_callback(callback);
}

void ModemCDMAProxy::set_signal_quality_callback(
    const SignalQualitySignalCallback &callback) {
  proxy_.set_signal_quality_callback(callback);
}

void ModemCDMAProxy::set_registration_state_callback(
    const RegistrationStateSignalCallback &callback) {
  proxy_.set_registration_state_callback(callback);
}

ModemCDMAProxy::Proxy::Proxy(DBus::Connection *connection,
                             const string &path,
                             const string &service)
    : DBus::ObjectProxy(*connection, path, service.c_str()) {}

ModemCDMAProxy::Proxy::~Proxy() {}

void ModemCDMAProxy::Proxy::set_activation_state_callback(
    const ActivationStateSignalCallback &callback) {
  activation_state_callback_ = callback;
}

void ModemCDMAProxy::Proxy::set_signal_quality_callback(
    const SignalQualitySignalCallback &callback) {
  signal_quality_callback_ = callback;
}

void ModemCDMAProxy::Proxy::set_registration_state_callback(
    const RegistrationStateSignalCallback &callback) {
  registration_state_callback_ = callback;
}

void ModemCDMAProxy::Proxy::ActivationStateChanged(
    const uint32 &activation_state,
    const uint32 &activation_error,
    const DBusPropertiesMap &status_changes) {
  SLOG(DBus, 2) << __func__ << "(" << activation_state << ", "
                << activation_error << ")";
  activation_state_callback_.Run(activation_state,
                                  activation_error,
                                  status_changes);
}

void ModemCDMAProxy::Proxy::SignalQuality(const uint32 &quality) {
  SLOG(DBus, 2) << __func__ << "(" << quality << ")";
  signal_quality_callback_.Run(quality);
}

void ModemCDMAProxy::Proxy::RegistrationStateChanged(
    const uint32 &cdma_1x_state,
    const uint32 &evdo_state) {
  SLOG(DBus, 2) << __func__ << "(" << cdma_1x_state << ", "
                << evdo_state << ")";
  registration_state_callback_.Run(cdma_1x_state, evdo_state);
}

void ModemCDMAProxy::Proxy::ActivateCallback(const uint32_t &status,
                                             const DBus::Error &dberror,
                                             void *data) {
  SLOG(DBus, 2) << __func__ << "(" << status << ")";
  scoped_ptr<ActivationResultCallback> callback(
      reinterpret_cast<ActivationResultCallback *>(data));
  Error error;
  CellularError::FromDBusError(dberror, &error);
  callback->Run(status, error);
}

void ModemCDMAProxy::Proxy::GetRegistrationStateCallback(
    const uint32 &state_1x, const uint32 &state_evdo,
    const DBus::Error &dberror, void *data) {
  SLOG(DBus, 2) << __func__ << "(" << state_1x << ", " << state_evdo << ")";
  scoped_ptr<RegistrationStateCallback> callback(
      reinterpret_cast<RegistrationStateCallback *>(data));
  Error error;
  CellularError::FromDBusError(dberror, &error);
  callback->Run(state_1x, state_evdo, error);
}


void ModemCDMAProxy::Proxy::GetSignalQualityCallback(const uint32 &quality,
                                                     const DBus::Error &dberror,
                                                     void *data) {
  SLOG(DBus, 2) << __func__ << "(" << quality << ")";
  scoped_ptr<SignalQualityCallback> callback(
      reinterpret_cast<SignalQualityCallback *>(data));
  Error error;
  CellularError::FromDBusError(dberror, &error);
  callback->Run(quality, error);
}

}  // namespace shill
