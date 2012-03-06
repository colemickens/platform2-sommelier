// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/mm1_modem_proxy.h"

#include <base/logging.h>

#include "cellular_error.h"

using std::string;

namespace shill {
namespace mm1 {

ModemProxy::ModemProxy(ModemProxyDelegate *delegate,
                       DBus::Connection *connection,
                       const string &path,
                       const string &service)
    : proxy_(delegate, connection, path, service) {}

ModemProxy::~ModemProxy() {}

void ModemProxy::Enable(const bool &enable,
                        AsyncCallHandler *call_handler,
                        int timeout) {
  proxy_.Enable(enable, call_handler, timeout);
}

void ModemProxy::ListBearers(AsyncCallHandler *call_handler,
                             int timeout) {
  proxy_.ListBearers(call_handler, timeout);
}

void ModemProxy::CreateBearer(
    const DBusPropertiesMap &properties,
    AsyncCallHandler *call_handler,
    int timeout) {
  proxy_.CreateBearer(properties, call_handler, timeout);
}

void ModemProxy::DeleteBearer(const ::DBus::Path &bearer,
                              AsyncCallHandler *call_handler,
                              int timeout) {
  proxy_.DeleteBearer(bearer, call_handler, timeout);
}

void ModemProxy::Reset(AsyncCallHandler *call_handler, int timeout) {
  proxy_.Reset(call_handler, timeout);
}

void ModemProxy::FactoryReset(const std::string &code,
                              AsyncCallHandler *call_handler,
                              int timeout) {
  proxy_.FactoryReset(code, call_handler, timeout);
}

void ModemProxy::SetAllowedModes(const uint32_t &modes,
                                 const uint32_t &preferred,
                                 AsyncCallHandler *call_handler,
                                 int timeout) {
  proxy_.SetAllowedModes(modes, preferred, call_handler, timeout);
}

void ModemProxy::SetBands(const std::vector< uint32_t > &bands,
                          AsyncCallHandler *call_handler,
                          int timeout) {
  proxy_.SetBands(bands, call_handler, timeout);
}

void ModemProxy::Command(const std::string &cmd,
                         const uint32_t &user_timeout,
                         AsyncCallHandler *call_handler,
                         int timeout) {
  proxy_.Command(cmd, user_timeout, call_handler, timeout);
}

// Inherited properties from ModemProxyInterface.
const ::DBus::Path ModemProxy::Sim() {
  return proxy_.Sim();
};
uint32_t ModemProxy::ModemCapabilities() {
  return proxy_.ModemCapabilities();
};
uint32_t ModemProxy::CurrentCapabilities() {
  return proxy_.CurrentCapabilities();
};
uint32_t ModemProxy::MaxBearers() {
  return proxy_.MaxBearers();
};
uint32_t ModemProxy::MaxActiveBearers() {
  return proxy_.MaxActiveBearers();
};
const std::string ModemProxy::Manufacturer() {
  return proxy_.Manufacturer();
};
const std::string ModemProxy::Model() {
  return proxy_.Model();
};
const std::string ModemProxy::Revision() {
  return proxy_.Revision();
};
const std::string ModemProxy::DeviceIdentifier() {
  return proxy_.DeviceIdentifier();
};
const std::string ModemProxy::Device() {
  return proxy_.Device();
};
const std::string ModemProxy::Driver() {
  return proxy_.Driver();
};
const std::string ModemProxy::Plugin() {
  return proxy_.Plugin();
};
const std::string ModemProxy::EquipmentIdentifier() {
  return proxy_.EquipmentIdentifier();
};
uint32_t ModemProxy::UnlockRequired() {
  return proxy_.UnlockRequired();
};
const std::map< uint32_t, uint32_t > ModemProxy::UnlockRetries() {
  return proxy_.UnlockRetries();
};
uint32_t ModemProxy::State() {
  return proxy_.State();
};
uint32_t ModemProxy::AccessTechnologies() {
  return proxy_.AccessTechnologies();
};
const ::DBus::Struct< uint32_t, bool > ModemProxy::SignalQuality() {
  return proxy_.SignalQuality();
};
uint32_t ModemProxy::SupportedModes() {
  return proxy_.SupportedModes();
};
uint32_t ModemProxy::AllowedModes() {
  return proxy_.AllowedModes();
};
uint32_t ModemProxy::PreferredMode() {
  return proxy_.PreferredMode();
};
const std::vector< uint32_t > ModemProxy::SupportedBands() {
  return proxy_.SupportedBands();
};
const std::vector< uint32_t > ModemProxy::Bands() {
  return proxy_.Bands();
};

ModemProxy::Proxy::Proxy(ModemProxyDelegate *delegate,
                         DBus::Connection *connection,
                         const std::string &path,
                         const std::string &service)
    : DBus::ObjectProxy(*connection, path, service.c_str()),
      delegate_(delegate) {}

ModemProxy::Proxy::~Proxy() {}

// Signal callbacks inherited from Proxy
void ModemProxy::Proxy::StateChanged(const uint32_t &old,
                                     const uint32_t &_new,
                                     const uint32_t &reason) {
  delegate_->OnStateChanged(old, _new, reason);
}

// Method callbacks inherited from
// org::freedesktop::ModemManager1::ModemProxy
void ModemProxy::Proxy::EnableCallback(const ::DBus::Error& dberror,
                                       void *data) {
  AsyncCallHandler *call_handler = reinterpret_cast<AsyncCallHandler *>(data);
  Error error;
  CellularError::FromDBusError(dberror, &error),
      delegate_->OnEnableCallback(error, call_handler);
}

void ModemProxy::Proxy::ListBearersCallback(
    const std::vector< ::DBus::Path > &bearers,
    const ::DBus::Error& dberror,
    void *data) {
  AsyncCallHandler *call_handler = reinterpret_cast<AsyncCallHandler *>(data);
  Error error;
  CellularError::FromDBusError(dberror, &error),
      delegate_->OnListBearersCallback(bearers, error, call_handler);
}

void ModemProxy::Proxy::CreateBearerCallback(const ::DBus::Path &path,
                                             const ::DBus::Error& dberror,
                                             void *data) {
  AsyncCallHandler *call_handler = reinterpret_cast<AsyncCallHandler *>(data);
  Error error;
  CellularError::FromDBusError(dberror, &error),
      delegate_->OnCreateBearerCallback(path, error, call_handler);
}

void ModemProxy::Proxy::DeleteBearerCallback(const ::DBus::Error& dberror,
                                             void *data) {
  AsyncCallHandler *call_handler = reinterpret_cast<AsyncCallHandler *>(data);
  Error error;
  CellularError::FromDBusError(dberror, &error),
      delegate_->OnDeleteBearerCallback(error, call_handler);
}

void ModemProxy::Proxy::ResetCallback(const ::DBus::Error& dberror,
                                      void *data) {
  AsyncCallHandler *call_handler = reinterpret_cast<AsyncCallHandler *>(data);
  Error error;
  CellularError::FromDBusError(dberror, &error),
      delegate_->OnResetCallback(error, call_handler);
}

void ModemProxy::Proxy::FactoryResetCallback(const ::DBus::Error& dberror,
                                             void *data) {
  AsyncCallHandler *call_handler = reinterpret_cast<AsyncCallHandler *>(data);
  Error error;
  CellularError::FromDBusError(dberror, &error),
      delegate_->OnFactoryResetCallback(error, call_handler);
}

void ModemProxy::Proxy::SetAllowedModesCallback(
    const ::DBus::Error& dberror,
    void *data) {
  AsyncCallHandler *call_handler = reinterpret_cast<AsyncCallHandler *>(data);
  Error error;
  CellularError::FromDBusError(dberror, &error),
      delegate_->OnSetAllowedModesCallback(error, call_handler);
}

void ModemProxy::Proxy::SetBandsCallback(const ::DBus::Error& dberror,
                                         void *data) {
  AsyncCallHandler *call_handler = reinterpret_cast<AsyncCallHandler *>(data);
  Error error;
  CellularError::FromDBusError(dberror, &error),
      delegate_->OnSetBandsCallback(error, call_handler);
}

void ModemProxy::Proxy::CommandCallback(const std::string &response,
                                        const ::DBus::Error& dberror,
                                        void *data) {
  AsyncCallHandler *call_handler = reinterpret_cast<AsyncCallHandler *>(data);
  Error error;
  CellularError::FromDBusError(dberror, &error),
      delegate_->OnCommandCallback(response, error, call_handler);
}

}  // namespace mm1
}  // namespace shill
