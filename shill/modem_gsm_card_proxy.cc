// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/modem_gsm_card_proxy.h"

#include "shill/cellular_error.h"
#include "shill/error.h"

using std::string;

namespace shill {

ModemGSMCardProxy::ModemGSMCardProxy(ModemGSMCardProxyDelegate *delegate,
                                     DBus::Connection *connection,
                                     const string &path,
                                     const string &service)
    : proxy_(delegate, connection, path, service) {}

ModemGSMCardProxy::~ModemGSMCardProxy() {}

void ModemGSMCardProxy::GetIMEI(AsyncCallHandler *call_handler, int timeout) {
  proxy_.GetImei(call_handler, timeout);
}

void ModemGSMCardProxy::GetIMSI(AsyncCallHandler *call_handler, int timeout) {
  proxy_.GetImsi(call_handler, timeout);
}

void ModemGSMCardProxy::GetSPN(AsyncCallHandler *call_handler, int timeout) {
  proxy_.GetSpn(call_handler, timeout);
}

void ModemGSMCardProxy::GetMSISDN(AsyncCallHandler *call_handler, int timeout) {
  proxy_.GetMsIsdn(call_handler, timeout);
}

void ModemGSMCardProxy::EnablePIN(const string &pin, bool enabled,
                                  AsyncCallHandler *call_handler, int timeout) {
  proxy_.EnablePin(pin, enabled, call_handler, timeout);
}

void ModemGSMCardProxy::SendPIN(const string &pin,
                                AsyncCallHandler *call_handler, int timeout) {
  proxy_.SendPin(pin, call_handler, timeout);
}

void ModemGSMCardProxy::SendPUK(const string &puk, const string &pin,
                                AsyncCallHandler *call_handler, int timeout) {
  proxy_.SendPuk(puk, pin, call_handler, timeout);
}

void ModemGSMCardProxy::ChangePIN(const string &old_pin,
                                  const string &new_pin,
                                  AsyncCallHandler *call_handler, int timeout) {
  proxy_.ChangePin(old_pin, new_pin, call_handler, timeout);
}

uint32 ModemGSMCardProxy::EnabledFacilityLocks() {
  return proxy_.EnabledFacilityLocks();
}

ModemGSMCardProxy::Proxy::Proxy(ModemGSMCardProxyDelegate *delegate,
                                DBus::Connection *connection,
                                const string &path,
                                const string &service)
    : DBus::ObjectProxy(*connection, path, service.c_str()),
      delegate_(delegate) {}

void ModemGSMCardProxy::Proxy::GetImeiCallback(const string &imei,
                                               const DBus::Error &dberror,
                                               void *data) {
  AsyncCallHandler *call_handler = reinterpret_cast<AsyncCallHandler *>(data);
  Error error;
  CellularError::FromDBusError(dberror, &error);
  delegate_->OnGetIMEICallback(imei, error, call_handler);
}

void ModemGSMCardProxy::Proxy::GetImsiCallback(const string &imsi,
                                               const DBus::Error &dberror,
                                               void *data) {
  AsyncCallHandler *call_handler = reinterpret_cast<AsyncCallHandler *>(data);
  Error error;
  CellularError::FromDBusError(dberror, &error);
  delegate_->OnGetIMSICallback(imsi, error, call_handler);
}

void ModemGSMCardProxy::Proxy::GetSpnCallback(const string &spn,
                                              const DBus::Error &dberror,
                                              void *data) {
  AsyncCallHandler *call_handler = reinterpret_cast<AsyncCallHandler *>(data);
  Error error;
  CellularError::FromDBusError(dberror, &error);
  delegate_->OnGetSPNCallback(spn, error, call_handler);
}

void ModemGSMCardProxy::Proxy::GetMsIsdnCallback(const string &misisdn,
                                                 const DBus::Error &dberror,
                                                 void *data) {
  AsyncCallHandler *call_handler = reinterpret_cast<AsyncCallHandler *>(data);
  Error error;
  CellularError::FromDBusError(dberror, &error);
  delegate_->OnGetMSISDNCallback(misisdn, error, call_handler);
}

void ModemGSMCardProxy::Proxy::EnablePinCallback(const DBus::Error &dberror,
                                                 void *data) {
  AsyncCallHandler *call_handler = reinterpret_cast<AsyncCallHandler *>(data);
  Error error;
  CellularError::FromDBusError(dberror, &error);
  delegate_->OnPINOperationCallback(error, call_handler);
}

void ModemGSMCardProxy::Proxy::SendPinCallback(const DBus::Error &dberror,
                                               void *data) {
  AsyncCallHandler *call_handler = reinterpret_cast<AsyncCallHandler *>(data);
  Error error;
  CellularError::FromDBusError(dberror, &error);
  delegate_->OnPINOperationCallback(error, call_handler);
}

void ModemGSMCardProxy::Proxy::SendPukCallback(const DBus::Error &dberror,
                                               void *data) {
  AsyncCallHandler *call_handler = reinterpret_cast<AsyncCallHandler *>(data);
  Error error;
  CellularError::FromDBusError(dberror, &error);
  delegate_->OnPINOperationCallback(error, call_handler);
}

void ModemGSMCardProxy::Proxy::ChangePinCallback(const DBus::Error &dberror,
                                                 void *data) {
  AsyncCallHandler *call_handler = reinterpret_cast<AsyncCallHandler *>(data);
  Error error;
  CellularError::FromDBusError(dberror, &error);
  delegate_->OnPINOperationCallback(error, call_handler);
}

ModemGSMCardProxy::Proxy::~Proxy() {}

}  // namespace shill
