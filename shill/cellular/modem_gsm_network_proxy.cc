// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/cellular/modem_gsm_network_proxy.h"

#include <memory>

#include "shill/cellular/cellular_error.h"
#include "shill/dbus_async_call_helper.h"
#include "shill/error.h"
#include "shill/logging.h"

using base::Callback;
using std::string;
using std::unique_ptr;

namespace shill {

ModemGSMNetworkProxy::ModemGSMNetworkProxy(
    DBus::Connection* connection,
    const string& path,
    const string& service)
    : proxy_(connection, path, service) {}

ModemGSMNetworkProxy::~ModemGSMNetworkProxy() {}

void ModemGSMNetworkProxy::GetRegistrationInfo(
    Error* error,
    const RegistrationInfoCallback& callback,
    int timeout) {
  BeginAsyncDBusCall(__func__, proxy_, &Proxy::GetRegistrationInfoAsync,
                     callback, error, &CellularError::FromDBusError, timeout);
}

void ModemGSMNetworkProxy::GetSignalQuality(
    Error* error,
    const SignalQualityCallback& callback,
    int timeout) {
  BeginAsyncDBusCall(__func__, proxy_, &Proxy::GetSignalQualityAsync, callback,
                     error, &CellularError::FromDBusError, timeout);
}

void ModemGSMNetworkProxy::Register(const string& network_id,
                                    Error* error,
                                    const ResultCallback& callback,
                                    int timeout) {
  BeginAsyncDBusCall(__func__, proxy_, &Proxy::RegisterAsync, callback,
                     error, &CellularError::FromDBusError, timeout,
                     network_id);
}

void ModemGSMNetworkProxy::Scan(Error* error,
                                const ScanResultsCallback& callback,
                                int timeout) {
  BeginAsyncDBusCall(__func__, proxy_, &Proxy::ScanAsync, callback,
                     error, &CellularError::FromDBusError, timeout);
}

uint32_t ModemGSMNetworkProxy::AccessTechnology() {
  SLOG(&proxy_.path(), 2) << __func__;
  try {
  return proxy_.AccessTechnology();
  } catch (const DBus::Error& e) {
    LOG(FATAL) << "DBus exception: " << e.name() << ": " << e.what();
    return 0;  // Make the compiler happy.
  }
}

void ModemGSMNetworkProxy::set_signal_quality_callback(
    const SignalQualitySignalCallback& callback) {
  proxy_.set_signal_quality_callback(callback);
}

void ModemGSMNetworkProxy::set_network_mode_callback(
    const NetworkModeSignalCallback& callback) {
  proxy_.set_network_mode_callback(callback);
}

void ModemGSMNetworkProxy::set_registration_info_callback(
    const RegistrationInfoSignalCallback& callback) {
  proxy_.set_registration_info_callback(callback);
}

ModemGSMNetworkProxy::Proxy::Proxy(DBus::Connection* connection,
                                   const string& path,
                                   const string& service)
    : DBus::ObjectProxy(*connection, path, service.c_str()) {}

ModemGSMNetworkProxy::Proxy::~Proxy() {}

void ModemGSMNetworkProxy::Proxy::set_signal_quality_callback(
    const SignalQualitySignalCallback& callback) {
  signal_quality_callback_ = callback;
}

void ModemGSMNetworkProxy::Proxy::set_network_mode_callback(
    const NetworkModeSignalCallback& callback) {
  network_mode_callback_ = callback;
}

void ModemGSMNetworkProxy::Proxy::set_registration_info_callback(
    const RegistrationInfoSignalCallback& callback) {
  registration_info_callback_ = callback;
}

void ModemGSMNetworkProxy::Proxy::SignalQuality(const uint32_t& quality) {
  SLOG(&path(), 2) << __func__ << "(" << quality << ")";
  if (!signal_quality_callback_.is_null())
    signal_quality_callback_.Run(quality);
}

void ModemGSMNetworkProxy::Proxy::RegistrationInfo(
    const uint32_t& status,
    const string& operator_code,
    const string& operator_name) {
  SLOG(&path(), 2) << __func__ << "(" << status << ", " << operator_code << ", "
                 << operator_name << ")";
  if (!registration_info_callback_.is_null())
    registration_info_callback_.Run(status, operator_code, operator_name);
}

void ModemGSMNetworkProxy::Proxy::NetworkMode(const uint32_t& mode) {
  SLOG(&path(), 2) << __func__ << "(" << mode << ")";
  if (!network_mode_callback_.is_null())
    network_mode_callback_.Run(mode);
}

void ModemGSMNetworkProxy::Proxy::RegisterCallback(const DBus::Error& dberror,
                                                   void* data) {
  SLOG(&path(), 2) << __func__;
  unique_ptr<ResultCallback> callback(reinterpret_cast<ResultCallback*>(data));
  Error error;
  CellularError::FromDBusError(dberror, &error);
  callback->Run(error);
}

void ModemGSMNetworkProxy::Proxy::GetRegistrationInfoCallback(
    const GSMRegistrationInfo& info, const DBus::Error& dberror, void* data) {
  SLOG(&path(), 2) << __func__;
  unique_ptr<RegistrationInfoCallback> callback(
      reinterpret_cast<RegistrationInfoCallback*>(data));
  Error error;
  CellularError::FromDBusError(dberror, &error);
  callback->Run(info._1, info._2, info._3, error);
}

void ModemGSMNetworkProxy::Proxy::GetSignalQualityCallback(
    const uint32_t& quality, const DBus::Error& dberror, void* data) {
  SLOG(&path(), 2) << __func__ << "(" << quality << ")";
  unique_ptr<SignalQualityCallback> callback(
      reinterpret_cast<SignalQualityCallback*>(data));
  Error error;
  CellularError::FromDBusError(dberror, &error);
  callback->Run(quality, error);
}

void ModemGSMNetworkProxy::Proxy::ScanCallback(const GSMScanResults& results,
                                               const DBus::Error& dberror,
                                               void* data) {
  SLOG(&path(), 2) << __func__;
  unique_ptr<ScanResultsCallback> callback(
      reinterpret_cast<ScanResultsCallback*>(data));
  Error error;
  CellularError::FromDBusError(dberror, &error);
  callback->Run(results, error);
}

}  // namespace shill
