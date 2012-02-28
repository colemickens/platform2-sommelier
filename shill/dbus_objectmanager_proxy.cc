// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "shill/dbus_objectmanager_proxy.h"

#include <base/logging.h>

#include "cellular_error.h"

using std::string;

namespace shill {

DBusObjectManagerProxy::DBusObjectManagerProxy(
    DBusObjectManagerProxyDelegate *delegate,
    DBus::Connection *connection,
    const string &path,
    const string &service)
    : proxy_(delegate, connection, path, service) {}

DBusObjectManagerProxy::~DBusObjectManagerProxy() {}

void DBusObjectManagerProxy::GetManagedObjects(AsyncCallHandler *call_handler,
                                               int timeout) {
  return proxy_.GetManagedObjects(call_handler, timeout);
}

// Inherited from DBusObjectManagerProxyInterface.
DBusObjectManagerProxy::Proxy::Proxy(DBusObjectManagerProxyDelegate *delegate,
                                     DBus::Connection *connection,
                                     const std::string &path,
                                     const std::string &service)
    : DBus::ObjectProxy(*connection, path, service.c_str()),
      delegate_(delegate) {}


DBusObjectManagerProxy::Proxy::~Proxy() {}

// Signal callback
void DBusObjectManagerProxy::Proxy::InterfacesAdded(
    const ::DBus::Path &object_path,
    const DBusInterfaceToProperties &interface_to_properties) {
  VLOG(2) << __func__ << "(" << object_path << ")";
  delegate_->OnInterfacesAdded(object_path, interface_to_properties);
}

// Signal callback
void DBusObjectManagerProxy::Proxy::InterfacesRemoved(
    const ::DBus::Path &object_path,
    const std::vector< std::string > &interfaces) {
  VLOG(2) << __func__ << "(" << object_path << ")";
  delegate_->OnInterfacesRemoved(object_path, interfaces);
}

// Method callback
void DBusObjectManagerProxy::Proxy::GetManagedObjectsCallback(
    const DBusObjectsWithProperties &objects_with_properties,
    const DBus::Error &dberror,
    void *data) {
  VLOG(2) << __func__;
  shill::Error error;
  CellularError::FromDBusError(dberror, &error);
  AsyncCallHandler *call_handler = reinterpret_cast<AsyncCallHandler *>(data);

  delegate_->OnGetManagedObjectsCallback(
      objects_with_properties,
      error,
      call_handler);
}

}  // namespace shill
