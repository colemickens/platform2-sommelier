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

#include "shill/cellular/mm1_sim_proxy.h"

#include <memory>

#include "shill/cellular/cellular_error.h"
#include "shill/dbus_async_call_helper.h"
#include "shill/logging.h"

using std::string;
using std::unique_ptr;

namespace shill {

namespace mm1 {

template<typename TraceMsgT, typename CallT, typename CallbackT,
         typename... ArgTypes>
void SimProxy::BeginCall(
    const TraceMsgT& trace_msg, const CallT& call, const CallbackT& callback,
    Error* error, int timeout, ArgTypes... rest) {
  BeginAsyncDBusCall(trace_msg, proxy_, call, callback, error,
                     &CellularError::FromMM1DBusError, timeout, rest...);
}

SimProxy::SimProxy(DBus::Connection* connection,
                   const string& path,
                   const string& service)
    : proxy_(connection, path, service) {}

SimProxy::~SimProxy() {}


void SimProxy::SendPin(const string& pin,
                       Error* error,
                       const ResultCallback& callback,
                       int timeout) {
  // pin is intentionally not logged.
  SLOG(&proxy_.path(), 2) << __func__ << "( XXX, " << timeout << ")";
  BeginCall(__func__, &Proxy::SendPinAsync, callback, error, timeout,
            pin);
}

void SimProxy::SendPuk(const string& puk,
                       const string& pin,
                       Error* error,
                       const ResultCallback& callback,
                       int timeout) {
  // pin and puk are intentionally not logged.
  SLOG(&proxy_.path(), 2) << __func__ << "( XXX, XXX, " << timeout << ")";
  BeginCall(__func__, &Proxy::SendPukAsync, callback, error, timeout,
            puk, pin);
}

void SimProxy::EnablePin(const string& pin,
                         const bool enabled,
                         Error* error,
                         const ResultCallback& callback,
                         int timeout) {
  // pin is intentionally not logged.
  SLOG(&proxy_.path(), 2) << __func__ << "( XXX, "
                          << enabled << ", " << timeout << ")";
  BeginCall(__func__, &Proxy::EnablePinAsync, callback, error, timeout,
            pin, enabled);
}

void SimProxy::ChangePin(const string& old_pin,
                         const string& new_pin,
                         Error* error,
                         const ResultCallback& callback,
                         int timeout) {
  // old_pin and new_pin are intentionally not logged.
  SLOG(&proxy_.path(), 2) << __func__ << "( XXX, XXX, " << timeout << ")";
  BeginCall(__func__, &Proxy::ChangePinAsync, callback, error, timeout,
            old_pin, new_pin);
}

SimProxy::Proxy::Proxy(DBus::Connection* connection,
                       const string& path,
                       const string& service)
    : DBus::ObjectProxy(*connection, path, service.c_str()) {}

SimProxy::Proxy::~Proxy() {}

// Method callbacks inherited from
// org::freedesktop::ModemManager1::SimProxy
void SimProxy::Proxy::SendPinCallback(const ::DBus::Error& dberror,
                                      void* data) {
  SLOG(DBus, &path(), 2) << __func__;
  unique_ptr<ResultCallback> callback(reinterpret_cast<ResultCallback*>(data));
  Error error;
  CellularError::FromMM1DBusError(dberror, &error);
  callback->Run(error);
}

void SimProxy::Proxy::SendPukCallback(const ::DBus::Error& dberror,
                                      void* data)  {
  SLOG(DBus, &path(), 2) << __func__;
  unique_ptr<ResultCallback> callback(reinterpret_cast<ResultCallback*>(data));
  Error error;
  CellularError::FromMM1DBusError(dberror, &error);
  callback->Run(error);
}

void SimProxy::Proxy::EnablePinCallback(const ::DBus::Error& dberror,
                                        void* data)  {
  SLOG(DBus, &path(), 2) << __func__;
  unique_ptr<ResultCallback> callback(reinterpret_cast<ResultCallback*>(data));
  Error error;
  CellularError::FromMM1DBusError(dberror, &error);
  callback->Run(error);
}

void SimProxy::Proxy::ChangePinCallback(const ::DBus::Error& dberror,
                                        void* data) {
  SLOG(DBus, &path(), 2) << __func__;
  unique_ptr<ResultCallback> callback(reinterpret_cast<ResultCallback*>(data));
  Error error;
  CellularError::FromMM1DBusError(dberror, &error);
  callback->Run(error);
}

}  // namespace mm1
}  // namespace shill
