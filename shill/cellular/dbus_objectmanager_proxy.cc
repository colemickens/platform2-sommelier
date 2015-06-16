// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/cellular/dbus_objectmanager_proxy.h"

#include <memory>

#include "shill/cellular/cellular_error.h"
#include "shill/dbus_async_call_helper.h"
#include "shill/logging.h"

using std::string;

namespace shill {

DBusObjectManagerProxy::DBusObjectManagerProxy(
    DBus::Connection* connection,
    const string& path,
    const string& service)
    : proxy_(connection, path, service) {}
DBusObjectManagerProxy::~DBusObjectManagerProxy() {}

void DBusObjectManagerProxy::GetManagedObjects(
    Error* error,
    const ManagedObjectsCallback& callback,
    int timeout) {
  BeginAsyncDBusCall(__func__, proxy_, &Proxy::GetManagedObjectsAsync, callback,
                     error, &CellularError::FromDBusError, timeout);
}

void DBusObjectManagerProxy::set_interfaces_added_callback(
      const InterfacesAddedSignalCallback& callback) {
  proxy_.set_interfaces_added_callback(callback);
}

void DBusObjectManagerProxy::set_interfaces_removed_callback(
      const InterfacesRemovedSignalCallback& callback) {
  proxy_.set_interfaces_removed_callback(callback);
}

// Inherited from DBusObjectManagerProxyInterface.
DBusObjectManagerProxy::Proxy::Proxy(DBus::Connection* connection,
                                     const std::string& path,
                                     const std::string& service)
    : DBus::ObjectProxy(*connection, path, service.c_str()) {}

DBusObjectManagerProxy::Proxy::~Proxy() {}

void DBusObjectManagerProxy::Proxy::set_interfaces_added_callback(
      const InterfacesAddedSignalCallback& callback) {
  interfaces_added_callback_ = callback;
}

void DBusObjectManagerProxy::Proxy::set_interfaces_removed_callback(
      const InterfacesRemovedSignalCallback& callback) {
  interfaces_removed_callback_ = callback;
}

// Signal callback
void DBusObjectManagerProxy::Proxy::InterfacesAdded(
    const ::DBus::Path& object_path,
    const DBusInterfaceToProperties& interface_to_properties) {
  SLOG(&path(), 2) << __func__ << "(" << object_path << ")";
  interfaces_added_callback_.Run(object_path, interface_to_properties);
}

// Signal callback
void DBusObjectManagerProxy::Proxy::InterfacesRemoved(
    const ::DBus::Path& object_path,
    const std::vector<std::string>& interfaces) {
  SLOG(&path(), 2) << __func__ << "(" << object_path << ")";
  interfaces_removed_callback_.Run(object_path, interfaces);
}

// Method callback
void DBusObjectManagerProxy::Proxy::GetManagedObjectsCallback(
    const DBusObjectsWithProperties& objects_with_properties,
    const DBus::Error& dberror,
    void* data) {
  SLOG(&path(), 2) << __func__;
  shill::Error error;
  CellularError::FromDBusError(dberror, &error);
  std::unique_ptr<ManagedObjectsCallback> callback(
      reinterpret_cast<ManagedObjectsCallback*>(data));

  callback->Run(objects_with_properties, error);
}

}  // namespace shill
