// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/mm1_modem_proxy.h"

#include "shill/cellular_error.h"
#include "shill/logging.h"

using std::string;

namespace shill {
namespace mm1 {

ModemProxy::ModemProxy(DBus::Connection *connection,
                       const string &path,
                       const string &service)
    : proxy_(connection, path, service) {}

ModemProxy::~ModemProxy() {}

void ModemProxy::set_state_changed_callback(
      const ModemStateChangedSignalCallback &callback) {
  proxy_.set_state_changed_callback(callback);
}

void ModemProxy::Enable(bool enable,
                        Error *error,
                        const ResultCallback &callback,
                        int timeout) {
  SLOG(Modem, 2) << __func__ << "(" << enable << ", " << timeout << ")";
  scoped_ptr<ResultCallback> cb(new ResultCallback(callback));
  try {
    SLOG(DBus, 2) << __func__;
    proxy_.Enable(enable, cb.get(), timeout);
    cb.release();
  } catch (const DBus::Error &e) {
    if (error)
      CellularError::FromDBusError(e, error);
  }
}

void ModemProxy::ListBearers(Error *error,
                             const DBusPathsCallback &callback,
                             int timeout) {
  scoped_ptr<DBusPathsCallback> cb(new DBusPathsCallback(callback));
  try {
    SLOG(DBus, 2) << __func__;
    proxy_.ListBearers(cb.get(), timeout);
    cb.release();
  } catch (const DBus::Error &e) {
    if (error)
      CellularError::FromDBusError(e, error);
  }
}

void ModemProxy::CreateBearer(
    const DBusPropertiesMap &properties,
    Error *error,
    const DBusPathCallback &callback,
    int timeout) {
  scoped_ptr<DBusPathCallback> cb(new DBusPathCallback(callback));
  try {
    SLOG(DBus, 2) << __func__;
    proxy_.CreateBearer(properties, cb.get(), timeout);
    cb.release();
  } catch (const DBus::Error &e) {
    if (error)
      CellularError::FromDBusError(e, error);
  }
}

void ModemProxy::DeleteBearer(const ::DBus::Path &bearer,
                              Error *error,
                              const ResultCallback &callback,
                              int timeout) {
  scoped_ptr<ResultCallback> cb(new ResultCallback(callback));
  try {
    SLOG(DBus, 2) << __func__;
    proxy_.DeleteBearer(bearer, cb.get(), timeout);
    cb.release();
  } catch (const DBus::Error &e) {
    if (error)
      CellularError::FromDBusError(e, error);
  }
}

void ModemProxy::Reset(Error *error,
                       const ResultCallback &callback,
                       int timeout) {
  scoped_ptr<ResultCallback> cb(new ResultCallback(callback));
  try {
    SLOG(DBus, 2) << __func__;
    proxy_.Reset(cb.get(), timeout);
    cb.release();
  } catch (const DBus::Error &e) {
    if (error)
      CellularError::FromDBusError(e, error);
  }
}

void ModemProxy::FactoryReset(const std::string &code,
                              Error *error,
                              const ResultCallback &callback,
                              int timeout) {
  scoped_ptr<ResultCallback> cb(new ResultCallback(callback));
  try {
    SLOG(DBus, 2) << __func__;
    proxy_.FactoryReset(code, cb.get(), timeout);
    cb.release();
  } catch (const DBus::Error &e) {
    if (error)
      CellularError::FromDBusError(e, error);
  }
}

void ModemProxy::SetAllowedModes(const uint32_t &modes,
                                 const uint32_t &preferred,
                                 Error *error,
                                 const ResultCallback &callback,
                                 int timeout) {
  scoped_ptr<ResultCallback> cb(new ResultCallback(callback));
  try {
    SLOG(DBus, 2) << __func__;
    proxy_.SetAllowedModes(modes, preferred, cb.get(), timeout);
    cb.release();
  } catch (const DBus::Error &e) {
    if (error)
      CellularError::FromDBusError(e, error);
  }
}

void ModemProxy::SetBands(const std::vector<uint32_t> &bands,
                          Error *error,
                          const ResultCallback &callback,
                          int timeout) {
  scoped_ptr<ResultCallback> cb(new ResultCallback(callback));
  try {
    SLOG(DBus, 2) << __func__;
    proxy_.SetBands(bands, cb.get(), timeout);
    cb.release();
  } catch (const DBus::Error &e) {
    if (error)
      CellularError::FromDBusError(e, error);
  }
}

void ModemProxy::Command(const std::string &cmd,
                         const uint32_t &user_timeout,
                         Error *error,
                         const StringCallback &callback,
                         int timeout) {
  scoped_ptr<StringCallback> cb(new StringCallback(callback));
  try {
    SLOG(DBus, 2) << __func__;
    proxy_.Command(cmd, user_timeout, cb.get(), timeout);
    cb.release();
  } catch (const DBus::Error &e) {
    if (error)
      CellularError::FromDBusError(e, error);
  }
}

// Inherited properties from ModemProxyInterface.
const ::DBus::Path ModemProxy::Sim() {
  SLOG(DBus, 2) << __func__;
  try {
    return proxy_.Sim();
  } catch (const DBus::Error &e) {
    LOG(FATAL) << "DBus exception: " << e.name() << ": " << e.what();
    return ::DBus::Path();  // Make the compiler happy.
  }
}
uint32_t ModemProxy::ModemCapabilities() {
  SLOG(DBus, 2) << __func__;
  try {
    return proxy_.ModemCapabilities();
  } catch (const DBus::Error &e) {
    LOG(FATAL) << "DBus exception: " << e.name() << ": " << e.what();
    return 0;  // Make the compiler happy.
  }
}
uint32_t ModemProxy::CurrentCapabilities() {
  SLOG(DBus, 2) << __func__;
  try {
    return proxy_.CurrentCapabilities();
  } catch (const DBus::Error &e) {
    LOG(FATAL) << "DBus exception: " << e.name() << ": " << e.what();
    return 0;  // Make the compiler happy.
  }
}
uint32_t ModemProxy::MaxBearers() {
  SLOG(DBus, 2) << __func__;
  try {
    return proxy_.MaxBearers();
  } catch (const DBus::Error &e) {
    LOG(FATAL) << "DBus exception: " << e.name() << ": " << e.what();
    return 0;  // Make the compiler happy.
  }
}
uint32_t ModemProxy::MaxActiveBearers() {
  SLOG(DBus, 2) << __func__;
  try {
    return proxy_.MaxActiveBearers();
  } catch (const DBus::Error &e) {
    LOG(FATAL) << "DBus exception: " << e.name() << ": " << e.what();
    return 0;  // Make the compiler happy.
  }
}
const std::string ModemProxy::Manufacturer() {
  SLOG(DBus, 2) << __func__;
  try {
    return proxy_.Manufacturer();
  } catch (const DBus::Error &e) {
    LOG(FATAL) << "DBus exception: " << e.name() << ": " << e.what();
    return std::string();  // Make the compiler happy.
  }
}
const std::string ModemProxy::Model() {
  SLOG(DBus, 2) << __func__;
  try {
    return proxy_.Model();
  } catch (const DBus::Error &e) {
    LOG(FATAL) << "DBus exception: " << e.name() << ": " << e.what();
    return std::string();  // Make the compiler happy.
  }
}
const std::string ModemProxy::Revision() {
  SLOG(DBus, 2) << __func__;
  try {
    return proxy_.Revision();
  } catch (const DBus::Error &e) {
    LOG(FATAL) << "DBus exception: " << e.name() << ": " << e.what();
    return std::string();  // Make the compiler happy.
  }
}
const std::string ModemProxy::DeviceIdentifier() {
  SLOG(DBus, 2) << __func__;
  try {
    return proxy_.DeviceIdentifier();
  } catch (const DBus::Error &e) {
    LOG(FATAL) << "DBus exception: " << e.name() << ": " << e.what();
    return std::string();  // Make the compiler happy.
  }
}
const std::string ModemProxy::Device() {
  SLOG(DBus, 2) << __func__;
  try {
    return proxy_.Device();
  } catch (const DBus::Error &e) {
    LOG(FATAL) << "DBus exception: " << e.name() << ": " << e.what();
    return std::string();  // Make the compiler happy.
  }
}
const std::string ModemProxy::Driver() {
  SLOG(DBus, 2) << __func__;
  try {
    return proxy_.Driver();
  } catch (const DBus::Error &e) {
    LOG(FATAL) << "DBus exception: " << e.name() << ": " << e.what();
    return std::string();  // Make the compiler happy.
  }
}
const std::string ModemProxy::Plugin() {
  SLOG(DBus, 2) << __func__;
  try {
    return proxy_.Plugin();
  } catch (const DBus::Error &e) {
    LOG(FATAL) << "DBus exception: " << e.name() << ": " << e.what();
    return std::string();  // Make the compiler happy.
  }
}
const std::string ModemProxy::EquipmentIdentifier() {
  SLOG(DBus, 2) << __func__;
  try {
    return proxy_.EquipmentIdentifier();
  } catch (const DBus::Error &e) {
    LOG(FATAL) << "DBus exception: " << e.name() << ": " << e.what();
    return std::string();  // Make the compiler happy.
  }
}
uint32_t ModemProxy::UnlockRequired() {
  SLOG(DBus, 2) << __func__;
  try {
    return proxy_.UnlockRequired();
  } catch (const DBus::Error &e) {
    LOG(FATAL) << "DBus exception: " << e.name() << ": " << e.what();
    return 0;  // Make the compiler happy.
  }
}
const std::map<uint32_t, uint32_t> ModemProxy::UnlockRetries() {
  SLOG(DBus, 2) << __func__;
  try {
    return proxy_.UnlockRetries();
  } catch (const DBus::Error &e) {
    LOG(FATAL) << "DBus exception: " << e.name() << ": " << e.what();
    return std::map<uint32_t, uint32_t>();  // Make the compiler happy.
  }
}
uint32_t ModemProxy::State() {
  SLOG(DBus, 2) << __func__;
  try {
    return proxy_.State();
  } catch (const DBus::Error &e) {
    LOG(FATAL) << "DBus exception: " << e.name() << ": " << e.what();
    return 0;  // Make the compiler happy.
  }
}
uint32_t ModemProxy::AccessTechnologies() {
  SLOG(DBus, 2) << __func__;
  try {
    return proxy_.AccessTechnologies();
  } catch (const DBus::Error &e) {
    LOG(FATAL) << "DBus exception: " << e.name() << ": " << e.what();
    return 0;  // Make the compiler happy.
  }
}
const ::DBus::Struct<uint32_t, bool> ModemProxy::SignalQuality() {
  SLOG(DBus, 2) << __func__;
  try {
    return proxy_.SignalQuality();
  } catch (const DBus::Error &e) {
    LOG(FATAL) << "DBus exception: " << e.name() << ": " << e.what();
    return ::DBus::Struct<uint32_t, bool>();  // Make the compiler happy.
  }
}
const std::vector<string> ModemProxy::OwnNumbers() {
  SLOG(DBus, 2) << __func__;
  try {
    return proxy_.OwnNumbers();
  } catch (const DBus::Error &e) {
    LOG(FATAL) << "DBus exception: " << e.name() << ": " << e.what();
    return std::vector<string>();  // Make the compiler happy.
  }
}
uint32_t ModemProxy::SupportedModes() {
  SLOG(DBus, 2) << __func__;
  try {
    return proxy_.SupportedModes();
  } catch (const DBus::Error &e) {
    LOG(FATAL) << "DBus exception: " << e.name() << ": " << e.what();
    return 0;  // Make the compiler happy.
  }
}
uint32_t ModemProxy::AllowedModes() {
  SLOG(DBus, 2) << __func__;
  try {
    return proxy_.AllowedModes();
  } catch (const DBus::Error &e) {
    LOG(FATAL) << "DBus exception: " << e.name() << ": " << e.what();
    return 0;  // Make the compiler happy.
  }
}
uint32_t ModemProxy::PreferredMode() {
  SLOG(DBus, 2) << __func__;
  try {
    return proxy_.PreferredMode();
  } catch (const DBus::Error &e) {
    LOG(FATAL) << "DBus exception: " << e.name() << ": " << e.what();
    return 0;  // Make the compiler happy.
  }
}
const std::vector<uint32_t> ModemProxy::SupportedBands() {
  SLOG(DBus, 2) << __func__;
  try {
    return proxy_.SupportedBands();
  } catch (const DBus::Error &e) {
    LOG(FATAL) << "DBus exception: " << e.name() << ": " << e.what();
    return std::vector<uint32_t>();  // Make the compiler happy.
  }
}
const std::vector<uint32_t> ModemProxy::Bands() {
  SLOG(DBus, 2) << __func__;
  try {
    return proxy_.Bands();
  } catch (const DBus::Error &e) {
    LOG(FATAL) << "DBus exception: " << e.name() << ": " << e.what();
    return std::vector<uint32_t>();  // Make the compiler happy.
  }
}

ModemProxy::Proxy::Proxy(DBus::Connection *connection,
                         const std::string &path,
                         const std::string &service)
    : DBus::ObjectProxy(*connection, path, service.c_str()) {}

ModemProxy::Proxy::~Proxy() {}

void ModemProxy::Proxy::set_state_changed_callback(
      const ModemStateChangedSignalCallback &callback) {
  state_changed_callback_ = callback;
}

// Signal callbacks inherited from Proxy
void ModemProxy::Proxy::StateChanged(const int32_t &old,
                                     const int32_t &_new,
                                     const uint32_t &reason) {
  SLOG(DBus, 2) << __func__;
  if (!state_changed_callback_.is_null())
    state_changed_callback_.Run(old, _new, reason);
}

// Method callbacks inherited from
// org::freedesktop::ModemManager1::ModemProxy
void ModemProxy::Proxy::EnableCallback(const ::DBus::Error& dberror,
                                       void *data) {
  SLOG(DBus, 2) << __func__;
  scoped_ptr<ResultCallback> callback(reinterpret_cast<ResultCallback *>(data));
  Error error;
  CellularError::FromDBusError(dberror, &error);
  callback->Run(error);
}

void ModemProxy::Proxy::ListBearersCallback(
    const std::vector< ::DBus::Path> &bearers,
    const ::DBus::Error& dberror,
    void *data) {
  SLOG(DBus, 2) << __func__;
  scoped_ptr<DBusPathsCallback> callback(
      reinterpret_cast<DBusPathsCallback *>(data));
  Error error;
  CellularError::FromDBusError(dberror, &error);
  callback->Run(bearers, error);
}

void ModemProxy::Proxy::CreateBearerCallback(const ::DBus::Path &path,
                                             const ::DBus::Error& dberror,
                                             void *data) {
  SLOG(DBus, 2) << __func__;
  scoped_ptr<ResultCallback> callback(reinterpret_cast<ResultCallback *>(data));
  Error error;
  CellularError::FromDBusError(dberror, &error);
  callback->Run(error);
}

void ModemProxy::Proxy::DeleteBearerCallback(const ::DBus::Error& dberror,
                                             void *data) {
  SLOG(DBus, 2) << __func__;
  scoped_ptr<ResultCallback> callback(reinterpret_cast<ResultCallback *>(data));
  Error error;
  CellularError::FromDBusError(dberror, &error);
  callback->Run(error);
}

void ModemProxy::Proxy::ResetCallback(const ::DBus::Error& dberror,
                                      void *data) {
  SLOG(DBus, 2) << __func__;
  scoped_ptr<ResultCallback> callback(reinterpret_cast<ResultCallback *>(data));
  Error error;
  CellularError::FromDBusError(dberror, &error);
  callback->Run(error);
}

void ModemProxy::Proxy::FactoryResetCallback(const ::DBus::Error& dberror,
                                             void *data) {
  SLOG(DBus, 2) << __func__;
  scoped_ptr<ResultCallback> callback(reinterpret_cast<ResultCallback *>(data));
  Error error;
  CellularError::FromDBusError(dberror, &error);
  callback->Run(error);
}

void ModemProxy::Proxy::SetAllowedModesCallback(
    const ::DBus::Error& dberror,
    void *data) {
  SLOG(DBus, 2) << __func__;
  scoped_ptr<ResultCallback> callback(reinterpret_cast<ResultCallback *>(data));
  Error error;
  CellularError::FromDBusError(dberror, &error);
  callback->Run(error);
}

void ModemProxy::Proxy::SetBandsCallback(const ::DBus::Error& dberror,
                                         void *data) {
  SLOG(DBus, 2) << __func__;
  scoped_ptr<ResultCallback> callback(reinterpret_cast<ResultCallback *>(data));
  Error error;
  CellularError::FromDBusError(dberror, &error);
  callback->Run(error);
}

void ModemProxy::Proxy::CommandCallback(const std::string &response,
                                        const ::DBus::Error& dberror,
                                        void *data) {
  SLOG(DBus, 2) << __func__;
  scoped_ptr<ResultCallback> callback(reinterpret_cast<ResultCallback *>(data));
  Error error;
  CellularError::FromDBusError(dberror, &error);
  callback->Run(error);
}

}  // namespace mm1
}  // namespace shill
