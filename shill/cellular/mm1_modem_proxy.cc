// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/cellular/mm1_modem_proxy.h"

#include <ModemManager/ModemManager.h>

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
void ModemProxy::BeginCall(
    const TraceMsgT& trace_msg, const CallT& call, const CallbackT& callback,
    Error* error, int timeout, ArgTypes... rest) {
  BeginAsyncDBusCall(trace_msg, proxy_, call, callback, error,
                     &CellularError::FromMM1DBusError, timeout, rest...);
}

ModemProxy::ModemProxy(DBus::Connection* connection,
                       const string& path,
                       const string& service)
    : proxy_(connection, path, service) {}

ModemProxy::~ModemProxy() {}

void ModemProxy::set_state_changed_callback(
    const ModemStateChangedSignalCallback& callback) {
  proxy_.set_state_changed_callback(callback);
}

void ModemProxy::Enable(bool enable,
                        Error* error,
                        const ResultCallback& callback,
                        int timeout) {
  SLOG(&proxy_.path(), 2) << __func__ << "(" << enable << ", "
                                      << timeout << ")";
  BeginCall(__func__, &Proxy::EnableAsync, callback, error, timeout,
            enable);
}

void ModemProxy::CreateBearer(
    const DBusPropertiesMap& properties,
    Error* error,
    const DBusPathCallback& callback,
    int timeout) {
  BeginCall(__func__, &Proxy::CreateBearerAsync, callback, error, timeout,
            properties);
}

void ModemProxy::DeleteBearer(const ::DBus::Path& bearer,
                              Error* error,
                              const ResultCallback& callback,
                              int timeout) {
  BeginCall(__func__, &Proxy::DeleteBearerAsync, callback, error, timeout,
            bearer);
}

void ModemProxy::Reset(Error* error,
                       const ResultCallback& callback,
                       int timeout) {
  BeginCall(__func__, &Proxy::ResetAsync, callback, error, timeout);
}

void ModemProxy::FactoryReset(const std::string& code,
                              Error* error,
                              const ResultCallback& callback,
                              int timeout) {
  BeginCall(__func__, &Proxy::FactoryResetAsync, callback, error, timeout,
            code);
}

void ModemProxy::SetCurrentCapabilities(const uint32_t& capabilities,
                                        Error* error,
                                        const ResultCallback& callback,
                                        int timeout) {
  BeginCall(__func__, &Proxy::SetCurrentCapabilitiesAsync, callback, error,
            timeout, capabilities);
}

void ModemProxy::SetCurrentModes(
    const ::DBus::Struct<uint32_t, uint32_t>& modes,
    Error* error,
    const ResultCallback& callback,
    int timeout) {
  BeginCall(__func__, &Proxy::SetCurrentModesAsync, callback, error, timeout,
            modes);
}

void ModemProxy::SetCurrentBands(const std::vector<uint32_t>& bands,
                                 Error* error,
                                 const ResultCallback& callback,
                                 int timeout) {
  BeginCall(__func__, &Proxy::SetCurrentBandsAsync, callback, error, timeout,
            bands);
}

void ModemProxy::Command(const std::string& cmd,
                         const uint32_t& user_timeout,
                         Error* error,
                         const StringCallback& callback,
                         int timeout) {
  BeginCall(__func__, &Proxy::CommandAsync, callback, error, timeout,
            cmd, user_timeout);
}

void ModemProxy::SetPowerState(const uint32_t& power_state,
                               Error* error,
                               const ResultCallback& callback,
                               int timeout) {
  BeginCall(__func__, &Proxy::SetPowerStateAsync, callback, error, timeout,
            power_state);
}

ModemProxy::Proxy::Proxy(DBus::Connection* connection,
                         const std::string& path,
                         const std::string& service)
    : DBus::ObjectProxy(*connection, path, service.c_str()) {}

ModemProxy::Proxy::~Proxy() {}

void ModemProxy::Proxy::set_state_changed_callback(
      const ModemStateChangedSignalCallback& callback) {
  state_changed_callback_ = callback;
}

// Signal callbacks inherited from Proxy
void ModemProxy::Proxy::StateChanged(const int32_t& old,
                                     const int32_t &_new,
                                     const uint32_t& reason) {
  SLOG(DBus, &path(), 2) << __func__;
  if (!state_changed_callback_.is_null())
    state_changed_callback_.Run(old, _new, reason);
}

// Method callbacks inherited from
// org::freedesktop::ModemManager1::ModemProxy
void ModemProxy::Proxy::EnableCallback(const ::DBus::Error& dberror,
                                       void* data) {
  SLOG(DBus, &path(), 2) << __func__;
  unique_ptr<ResultCallback> callback(reinterpret_cast<ResultCallback*>(data));
  Error error;
  CellularError::FromMM1DBusError(dberror, &error);
  callback->Run(error);
}

void ModemProxy::Proxy::CreateBearerCallback(const ::DBus::Path& path,
                                             const ::DBus::Error& dberror,
                                             void* data) {
  SLOG(DBus, &path, 2) << __func__;
  unique_ptr<ResultCallback> callback(reinterpret_cast<ResultCallback*>(data));
  Error error;
  CellularError::FromMM1DBusError(dberror, &error);
  callback->Run(error);
}

void ModemProxy::Proxy::DeleteBearerCallback(const ::DBus::Error& dberror,
                                             void* data) {
  SLOG(DBus, &path(), 2) << __func__;
  unique_ptr<ResultCallback> callback(reinterpret_cast<ResultCallback*>(data));
  Error error;
  CellularError::FromMM1DBusError(dberror, &error);
  callback->Run(error);
}

void ModemProxy::Proxy::ResetCallback(const ::DBus::Error& dberror,
                                      void* data) {
  SLOG(DBus, &path(), 2) << __func__;
  unique_ptr<ResultCallback> callback(reinterpret_cast<ResultCallback*>(data));
  Error error;
  CellularError::FromMM1DBusError(dberror, &error);
  callback->Run(error);
}

void ModemProxy::Proxy::FactoryResetCallback(const ::DBus::Error& dberror,
                                             void* data) {
  SLOG(DBus, &path(), 2) << __func__;
  unique_ptr<ResultCallback> callback(reinterpret_cast<ResultCallback*>(data));
  Error error;
  CellularError::FromMM1DBusError(dberror, &error);
  callback->Run(error);
}

void ModemProxy::Proxy::SetCurrentCapabilitesCallback(
    const ::DBus::Error& dberror,
    void* data) {
  SLOG(DBus, &path(), 2) << __func__;
  unique_ptr<ResultCallback> callback(reinterpret_cast<ResultCallback*>(data));
  Error error;
  CellularError::FromMM1DBusError(dberror, &error);
  callback->Run(error);
}

void ModemProxy::Proxy::SetCurrentModesCallback(const ::DBus::Error& dberror,
                                                void* data) {
  SLOG(DBus, &path(), 2) << __func__;
  unique_ptr<ResultCallback> callback(reinterpret_cast<ResultCallback*>(data));
  Error error;
  CellularError::FromMM1DBusError(dberror, &error);
  callback->Run(error);
}

void ModemProxy::Proxy::SetCurrentBandsCallback(const ::DBus::Error& dberror,
                                                void* data) {
  SLOG(DBus, &path(), 2) << __func__;
  unique_ptr<ResultCallback> callback(reinterpret_cast<ResultCallback*>(data));
  Error error;
  CellularError::FromMM1DBusError(dberror, &error);
  callback->Run(error);
}

void ModemProxy::Proxy::CommandCallback(const std::string& response,
                                        const ::DBus::Error& dberror,
                                        void* data) {
  SLOG(DBus, &path(), 2) << __func__;
  unique_ptr<ResultCallback> callback(reinterpret_cast<ResultCallback*>(data));
  Error error;
  CellularError::FromMM1DBusError(dberror, &error);
  callback->Run(error);
}

void ModemProxy::Proxy::SetPowerStateCallback(const ::DBus::Error& dberror,
                                              void* data) {
  SLOG(DBus, &path(), 2) << __func__;
  unique_ptr<ResultCallback> callback(reinterpret_cast<ResultCallback*>(data));
  Error error;
  CellularError::FromMM1DBusError(dberror, &error);
  callback->Run(error);
}

}  // namespace mm1
}  // namespace shill
