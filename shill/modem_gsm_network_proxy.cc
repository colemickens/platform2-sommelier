// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/modem_gsm_network_proxy.h"

#include <base/logging.h>

#include "shill/cellular_error.h"
#include "shill/error.h"
#include "shill/scope_logger.h"

using base::Callback;
using std::string;

namespace shill {

ModemGSMNetworkProxy::ModemGSMNetworkProxy(
    DBus::Connection *connection,
    const string &path,
    const string &service)
    : proxy_(connection, path, service) {}

ModemGSMNetworkProxy::~ModemGSMNetworkProxy() {}

void ModemGSMNetworkProxy::GetRegistrationInfo(
    Error *error,
    const RegistrationInfoCallback &callback,
    int timeout) {
  scoped_ptr<RegistrationInfoCallback>
      cb(new RegistrationInfoCallback(callback));
  try {
    SLOG(DBus, 2) << __func__;
    proxy_.GetRegistrationInfo(cb.get(), timeout);
    cb.release();
  } catch (DBus::Error e) {
    if (error)
      CellularError::FromDBusError(e, error);
  }
}

void ModemGSMNetworkProxy::GetSignalQuality(
    Error *error,
    const SignalQualityCallback &callback,
    int timeout) {
  scoped_ptr<SignalQualityCallback> cb(new SignalQualityCallback(callback));
  try {
    SLOG(DBus, 2) << __func__;
    proxy_.GetSignalQuality(cb.get(), timeout);
    cb.release();
  } catch (DBus::Error e) {
    if (error)
      CellularError::FromDBusError(e, error);
  }
}

void ModemGSMNetworkProxy::Register(const string &network_id,
                                    Error *error,
                                    const ResultCallback &callback,
                                    int timeout) {
  scoped_ptr<ResultCallback> cb(new ResultCallback(callback));
  try {
    SLOG(DBus, 2) << __func__;
    proxy_.Register(network_id, cb.get(), timeout);
    cb.release();
  } catch (DBus::Error e) {
    if (error)
      CellularError::FromDBusError(e, error);
  }
}

void ModemGSMNetworkProxy::Scan(Error *error,
                                const ScanResultsCallback &callback,
                                int timeout) {
  scoped_ptr<ScanResultsCallback> cb(new ScanResultsCallback(callback));
  try {
    SLOG(DBus, 2) << __func__;
    proxy_.Scan(cb.get(), timeout);
    cb.release();
  } catch (DBus::Error e) {
    if (error)
      CellularError::FromDBusError(e, error);
  }
}

uint32 ModemGSMNetworkProxy::AccessTechnology() {
  SLOG(DBus, 2) << __func__;
  return proxy_.AccessTechnology();
}

void ModemGSMNetworkProxy::set_signal_quality_callback(
    const SignalQualitySignalCallback &callback) {
  proxy_.set_signal_quality_callback(callback);
}

void ModemGSMNetworkProxy::set_network_mode_callback(
    const NetworkModeSignalCallback &callback) {
  proxy_.set_network_mode_callback(callback);
}

void ModemGSMNetworkProxy::set_registration_info_callback(
    const RegistrationInfoSignalCallback &callback) {
  proxy_.set_registration_info_callback(callback);
}

ModemGSMNetworkProxy::Proxy::Proxy(DBus::Connection *connection,
                                   const string &path,
                                   const string &service)
    : DBus::ObjectProxy(*connection, path, service.c_str()) {}

ModemGSMNetworkProxy::Proxy::~Proxy() {}

void ModemGSMNetworkProxy::Proxy::set_signal_quality_callback(
    const SignalQualitySignalCallback &callback) {
  signal_quality_callback_ = callback;
}

void ModemGSMNetworkProxy::Proxy::set_network_mode_callback(
    const NetworkModeSignalCallback &callback) {
  network_mode_callback_ = callback;
}

void ModemGSMNetworkProxy::Proxy::set_registration_info_callback(
    const RegistrationInfoSignalCallback &callback) {
  registration_info_callback_ = callback;
}

void ModemGSMNetworkProxy::Proxy::SignalQuality(const uint32 &quality) {
  SLOG(DBus, 2) << __func__ << "(" << quality << ")";
  if (!signal_quality_callback_.is_null())
    signal_quality_callback_.Run(quality);
}

void ModemGSMNetworkProxy::Proxy::RegistrationInfo(
    const uint32_t &status,
    const string &operator_code,
    const string &operator_name) {
  SLOG(DBus, 2) << __func__ << "(" << status << ", " << operator_code << ", "
                 << operator_name << ")";
  if (!registration_info_callback_.is_null())
    registration_info_callback_.Run(status, operator_code, operator_name);
}

void ModemGSMNetworkProxy::Proxy::NetworkMode(const uint32_t &mode) {
  SLOG(DBus, 2) << __func__ << "(" << mode << ")";
  if (!network_mode_callback_.is_null())
    network_mode_callback_.Run(mode);
}

void ModemGSMNetworkProxy::Proxy::RegisterCallback(const DBus::Error &dberror,
                                                   void *data) {
  SLOG(DBus, 2) << __func__;
  scoped_ptr<ResultCallback> callback(reinterpret_cast<ResultCallback *>(data));
  Error error;
  CellularError::FromDBusError(dberror, &error);
  callback->Run(error);
}

void ModemGSMNetworkProxy::Proxy::GetRegistrationInfoCallback(
    const GSMRegistrationInfo &info, const DBus::Error &dberror, void *data) {
  SLOG(DBus, 2) << __func__;
  scoped_ptr<RegistrationInfoCallback> callback(
      reinterpret_cast<RegistrationInfoCallback *>(data));
  Error error;
  CellularError::FromDBusError(dberror, &error);
  callback->Run(info._1, info._2, info._3, error);
}

void ModemGSMNetworkProxy::Proxy::GetSignalQualityCallback(
    const uint32 &quality, const DBus::Error &dberror, void *data) {
  SLOG(DBus, 2) << __func__ << "(" << quality << ")";
  scoped_ptr<SignalQualityCallback> callback(
      reinterpret_cast<SignalQualityCallback *>(data));
  Error error;
  CellularError::FromDBusError(dberror, &error);
  callback->Run(quality, error);
}

void ModemGSMNetworkProxy::Proxy::ScanCallback(const GSMScanResults &results,
                                               const DBus::Error &dberror,
                                               void *data) {
  SLOG(DBus, 2) << __func__;
  scoped_ptr<ScanResultsCallback> callback(
      reinterpret_cast<ScanResultsCallback *>(data));
  Error error;
  CellularError::FromDBusError(dberror, &error);
  callback->Run(results, error);
}

}  // namespace shill
