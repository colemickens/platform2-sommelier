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

#include "shill/cellular/modem_gsm_card_proxy.h"

#include <memory>

#include <base/bind.h>

#include "shill/cellular/cellular_error.h"
#include "shill/dbus_async_call_helper.h"
#include "shill/error.h"
#include "shill/logging.h"

using base::Bind;
using base::Callback;
using std::string;
using std::unique_ptr;

namespace shill {

template<typename TraceMsgT, typename CallT, typename CallbackT,
         typename... ArgTypes>
void ModemGSMCardProxy::BeginCall(
    const TraceMsgT& trace_msg, const CallT& call, const CallbackT& callback,
    Error* error, int timeout, ArgTypes... rest) {
  BeginAsyncDBusCall(trace_msg, proxy_, call, callback, error,
                     &CellularError::FromDBusError, timeout, rest...);
}

ModemGSMCardProxy::ModemGSMCardProxy(DBus::Connection* connection,
                                     const string& path,
                                     const string& service)
    : proxy_(connection, path, service) {}

ModemGSMCardProxy::~ModemGSMCardProxy() {}

void ModemGSMCardProxy::GetIMEI(Error* error,
                                const GSMIdentifierCallback& callback,
                                int timeout) {
  BeginCall(__func__, &Proxy::GetImeiAsync, callback, error, timeout);
}

void ModemGSMCardProxy::GetIMSI(Error* error,
                                const GSMIdentifierCallback& callback,
                                int timeout) {
  BeginCall(__func__, &Proxy::GetImsiAsync, callback, error, timeout);
}

void ModemGSMCardProxy::GetSPN(Error* error,
                               const GSMIdentifierCallback& callback,
                               int timeout) {
  BeginCall(__func__, &Proxy::GetSpnAsync, callback, error, timeout);
}

void ModemGSMCardProxy::GetMSISDN(Error* error,
                                  const GSMIdentifierCallback& callback,
                                  int timeout) {
  BeginCall(__func__, &Proxy::GetMsIsdnAsync, callback, error, timeout);
}

void ModemGSMCardProxy::EnablePIN(const string& pin, bool enabled,
                                  Error* error,
                                  const ResultCallback& callback,
                                  int timeout) {
  BeginCall(__func__, &Proxy::EnablePinAsync, callback, error, timeout,
            pin, enabled);
}

void ModemGSMCardProxy::SendPIN(const string& pin,
                                Error* error,
                                const ResultCallback& callback,
                                int timeout) {
  BeginCall(__func__, &Proxy::SendPinAsync, callback, error, timeout,
            pin);
}

void ModemGSMCardProxy::SendPUK(const string& puk, const string& pin,
                                Error* error,
                                const ResultCallback& callback,
                                int timeout) {
  BeginCall(__func__, &Proxy::SendPukAsync, callback, error, timeout,
            puk, pin);
}

void ModemGSMCardProxy::ChangePIN(const string& old_pin,
                                  const string& new_pin,
                                  Error* error,
                                  const ResultCallback& callback,
                                  int timeout) {
  BeginCall(__func__, &Proxy::ChangePinAsync, callback, error, timeout,
            old_pin, new_pin);
}

uint32_t ModemGSMCardProxy::EnabledFacilityLocks() {
  SLOG(&proxy_.path(), 2) << __func__;
  try {
    return proxy_.EnabledFacilityLocks();
  } catch (const DBus::Error& e) {
    LOG(FATAL) << "DBus exception: " << e.name() << ": " << e.what();
    return 0;  // Make the compiler happy.
  }
}

ModemGSMCardProxy::Proxy::Proxy(DBus::Connection* connection,
                                const string& path,
                                const string& service)
    : DBus::ObjectProxy(*connection, path, service.c_str()) {}

void ModemGSMCardProxy::Proxy::GetIdCallback(const string& id,
                                             const DBus::Error& dberror,
                                             void* data) {
  SLOG(&path(), 2) << __func__;
  unique_ptr<GSMIdentifierCallback> callback(
      reinterpret_cast<GSMIdentifierCallback*>(data));
  Error error;
  CellularError::FromDBusError(dberror, &error);
  callback->Run(id, error);
}

void ModemGSMCardProxy::Proxy::GetImeiCallback(const string& imei,
                                               const DBus::Error& dberror,
                                               void* data) {
  SLOG(&path(), 2) << __func__;
  GetIdCallback(imei, dberror, data);
}

void ModemGSMCardProxy::Proxy::GetImsiCallback(const string& imsi,
                                               const DBus::Error& dberror,
                                               void* data) {
  SLOG(&path(), 2) << __func__;
  GetIdCallback(imsi, dberror, data);
}

void ModemGSMCardProxy::Proxy::GetSpnCallback(const string& spn,
                                              const DBus::Error& dberror,
                                              void* data) {
  SLOG(&path(), 2) << __func__;
  GetIdCallback(spn, dberror, data);
}

void ModemGSMCardProxy::Proxy::GetMsIsdnCallback(const string& msisdn,
                                                 const DBus::Error& dberror,
                                                 void* data) {
  SLOG(&path(), 2) << __func__;
  GetIdCallback(msisdn, dberror, data);
}

void ModemGSMCardProxy::Proxy::PinCallback(const DBus::Error& dberror,
                                           void* data) {
  SLOG(&path(), 2) << __func__;
  unique_ptr<ResultCallback> callback(reinterpret_cast<ResultCallback*>(data));
  Error error;
  CellularError::FromDBusError(dberror, &error);
  callback->Run(error);
}

void ModemGSMCardProxy::Proxy::EnablePinCallback(const DBus::Error& dberror,
                                                 void* data) {
  SLOG(&path(), 2) << __func__;
  PinCallback(dberror, data);
}

void ModemGSMCardProxy::Proxy::SendPinCallback(const DBus::Error& dberror,
                                               void* data) {
  SLOG(&path(), 2) << __func__;
  PinCallback(dberror, data);
}

void ModemGSMCardProxy::Proxy::SendPukCallback(const DBus::Error& dberror,
                                               void* data) {
  SLOG(&path(), 2) << __func__;
  PinCallback(dberror, data);
}

void ModemGSMCardProxy::Proxy::ChangePinCallback(const DBus::Error& dberror,
                                                 void* data) {
  SLOG(&path(), 2) << __func__;
  PinCallback(dberror, data);
}

ModemGSMCardProxy::Proxy::~Proxy() {}

}  // namespace shill
