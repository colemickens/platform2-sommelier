// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/modem_gsm_network_proxy.h"

#include <base/logging.h>

#include "shill/cellular_error.h"
#include "shill/error.h"

using std::string;

namespace shill {

ModemGSMNetworkProxy::ModemGSMNetworkProxy(
    ModemGSMNetworkProxyDelegate *delegate,
    DBus::Connection *connection,
    const string &path,
    const string &service)
    : proxy_(delegate, connection, path, service) {}

ModemGSMNetworkProxy::~ModemGSMNetworkProxy() {}

void ModemGSMNetworkProxy::GetRegistrationInfo(AsyncCallHandler *call_handler,
                                               int timeout) {
  proxy_.GetRegistrationInfo(call_handler, timeout);
}

uint32 ModemGSMNetworkProxy::GetSignalQuality() {
  return proxy_.GetSignalQuality();
}

void ModemGSMNetworkProxy::Register(const string &network_id,
                                    AsyncCallHandler *call_handler,
                                    int timeout) {
  proxy_.Register(network_id, call_handler, timeout);
}

void ModemGSMNetworkProxy::Scan(AsyncCallHandler *call_handler, int timeout) {
  proxy_.Scan(call_handler, timeout);
}

uint32 ModemGSMNetworkProxy::AccessTechnology() {
  return proxy_.AccessTechnology();
}

ModemGSMNetworkProxy::Proxy::Proxy(ModemGSMNetworkProxyDelegate *delegate,
                                   DBus::Connection *connection,
                                   const string &path,
                                   const string &service)
    : DBus::ObjectProxy(*connection, path, service.c_str()),
      delegate_(delegate) {}

ModemGSMNetworkProxy::Proxy::~Proxy() {}

void ModemGSMNetworkProxy::Proxy::SignalQuality(const uint32 &quality) {
  VLOG(2) << __func__ << "(" << quality << ")";
  delegate_->OnGSMSignalQualityChanged(quality);
}

void ModemGSMNetworkProxy::Proxy::RegistrationInfo(
    const uint32_t &status,
    const string &operator_code,
    const string &operator_name) {
  VLOG(2) << __func__ << "(" << status << ", " << operator_code << ", "
          << operator_name << ")";
  delegate_->OnGSMRegistrationInfoChanged(status, operator_code, operator_name,
                                          Error(), NULL);
}

void ModemGSMNetworkProxy::Proxy::NetworkMode(const uint32_t &mode) {
  VLOG(2) << __func__ << "(" << mode << ")";
  delegate_->OnGSMNetworkModeChanged(mode);
}

void ModemGSMNetworkProxy::Proxy::RegisterCallback(const DBus::Error &dberror,
                                                   void *data) {
  AsyncCallHandler *call_handler = reinterpret_cast<AsyncCallHandler *>(data);
  Error error;
  CellularError::FromDBusError(dberror, &error);
  delegate_->OnRegisterCallback(error, call_handler);
}

void ModemGSMNetworkProxy::Proxy::GetRegistrationInfoCallback(
    const GSMRegistrationInfo &info, const DBus::Error &dberror, void *data) {
  AsyncCallHandler *call_handler = reinterpret_cast<AsyncCallHandler *>(data);
  Error error;
  CellularError::FromDBusError(dberror, &error);
  delegate_->OnGSMRegistrationInfoChanged(info._1, info._2, info._3,
                                          error, call_handler);
}

void ModemGSMNetworkProxy::Proxy::ScanCallback(const GSMScanResults &results,
                                               const DBus::Error &dberror,
                                               void *data) {
  AsyncCallHandler *call_handler = reinterpret_cast<AsyncCallHandler *>(data);
  Error error;
  CellularError::FromDBusError(dberror, &error);
  delegate_->OnScanCallback(results, error, call_handler);
}

}  // namespace shill
