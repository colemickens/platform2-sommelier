// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/mm1_bearer_proxy.h"

#include "shill/cellular_error.h"
#include "shill/logging.h"

using std::string;

namespace shill {
namespace mm1 {

BearerProxy::BearerProxy(DBus::Connection *connection,
                         const std::string &path,
                         const std::string &service)
    : proxy_(connection, path, service) {}

BearerProxy::~BearerProxy() {}


// Inherited methods from BearerProxyInterface
void BearerProxy::Connect(Error *error,
                          const ResultCallback &callback,
                          int timeout) {
  SLOG(Modem, 2) << __func__;
  scoped_ptr<ResultCallback> cb(new ResultCallback(callback));
  try {
    SLOG(DBus, 2) << __func__;
    proxy_.Connect(cb.get(), timeout);
    cb.release();
  } catch (const DBus::Error &e) {
    if (error)
      CellularError::FromDBusError(e, error);
  }
}

void BearerProxy::Disconnect(Error *error,
                             const ResultCallback &callback,
                             int timeout) {
  SLOG(Modem, 2) << __func__;
  scoped_ptr<ResultCallback> cb(new ResultCallback(callback));
  try {
    SLOG(DBus, 2) << __func__;
    proxy_.Disconnect(cb.get(), timeout);
    cb.release();
  } catch (const DBus::Error &e) {
    if (error)
      CellularError::FromDBusError(e, error);
  }
}

// Inherited properties from BearerProxyInterface
const string BearerProxy::Interface() {
  SLOG(DBus, 2) << __func__;
  try {
    return proxy_.Interface();
  } catch (const DBus::Error &e) {
    LOG(FATAL) << "DBus exception: " << e.name() << ": " << e.what();
    return string();  // Make the compiler happy.
  }
}

bool BearerProxy::Connected() {
  SLOG(DBus, 2) << __func__;
  try {
    return proxy_.Connected();
  } catch (const DBus::Error &e) {
    LOG(FATAL) << "DBus exception: " << e.name() << ": " << e.what();
    return false;  // Make the compiler happy.
  }
}

bool BearerProxy::Suspended() {
  SLOG(DBus, 2) << __func__;
  try {
    return proxy_.Suspended();
  } catch (const DBus::Error &e) {
    LOG(FATAL) << "DBus exception: " << e.name() << ": " << e.what();
    return false;  // Make the compiler happy.
  }
}

const DBusPropertiesMap BearerProxy::Ip4Config() {
  SLOG(DBus, 2) << __func__;
  try {
    return proxy_.Ip4Config();
  } catch (const DBus::Error &e) {
    LOG(FATAL) << "DBus exception: " << e.name() << ": " << e.what();
    return DBusPropertiesMap();  // Make the compiler happy.
  }
}

const DBusPropertiesMap BearerProxy::Ip6Config() {
  SLOG(DBus, 2) << __func__;
  try {
    return proxy_.Ip6Config();
  } catch (const DBus::Error &e) {
    LOG(FATAL) << "DBus exception: " << e.name() << ": " << e.what();
    return DBusPropertiesMap();  // Make the compiler happy.
  }
}

uint32_t BearerProxy::IpTimeout() {
  SLOG(DBus, 2) << __func__;
  try {
    return proxy_.IpTimeout();
  } catch (const DBus::Error &e) {
    LOG(FATAL) << "DBus exception: " << e.name() << ": " << e.what();
    return 0;  // Make the compiler happy.
  }
}

const DBusPropertiesMap BearerProxy::Properties() {
  SLOG(DBus, 2) << __func__;
  try {
    return proxy_.Properties();
  } catch (const DBus::Error &e) {
    LOG(FATAL) << "DBus exception: " << e.name() << ": " << e.what();
    return DBusPropertiesMap();  // Make the compiler happy.
  }
}

BearerProxy::Proxy::Proxy(DBus::Connection *connection,
                          const std::string &path,
                          const std::string &service)
    : DBus::ObjectProxy(*connection, path, service.c_str()) {}

BearerProxy::Proxy::~Proxy() {}


// Method callbacks inherited from
// org::freedesktop::ModemManager1::BearerProxy
void BearerProxy::Proxy::ConnectCallback(const ::DBus::Error &dberror,
                                         void *data) {
  SLOG(DBus, 2) << __func__;
  scoped_ptr<ResultCallback> callback(reinterpret_cast<ResultCallback *>(data));
  Error error;
  CellularError::FromDBusError(dberror, &error);
  callback->Run(error);
}

void BearerProxy::Proxy::DisconnectCallback(const ::DBus::Error &dberror,
                                            void *data) {
  SLOG(DBus, 2) << __func__;
  scoped_ptr<ResultCallback> callback(reinterpret_cast<ResultCallback *>(data));
  Error error;
  CellularError::FromDBusError(dberror, &error);
  callback->Run(error);
}

}  // namespace mm1
}  // namespace shill
