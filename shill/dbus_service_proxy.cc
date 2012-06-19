// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/dbus_service_proxy.h"

#include <chromeos/dbus/service_constants.h>

#include "shill/error.h"
#include "shill/scope_logger.h"

using std::string;

namespace shill {

DBusServiceProxy::DBusServiceProxy(DBus::Connection *connection)
    : proxy_(connection) {}

DBusServiceProxy::~DBusServiceProxy() {}

void DBusServiceProxy::GetNameOwner(const string &name,
                                    Error *error,
                                    const StringCallback &callback,
                                    int timeout) {
  SLOG(DBus, 2) << __func__ << "(" << name << ")";
  scoped_ptr<StringCallback> cb(new StringCallback(callback));
  try {
    proxy_.GetNameOwner(name, cb.get(), timeout);
    cb.release();
  } catch (const DBus::Error &e) {
    FromDBusError(e, error);
  }
}

void DBusServiceProxy::set_name_owner_changed_callback(
    const NameOwnerChangedCallback &callback) {
  proxy_.set_name_owner_changed_callback(callback);
}

// static
void DBusServiceProxy::FromDBusError(const DBus::Error &dbus_error,
                                     Error *error) {
  if (!error) {
    return;
  }
  if (!dbus_error.is_set()) {
    error->Reset();
    return;
  }
  Error::PopulateAndLog(error, Error::kOperationFailed, dbus_error.what());
}

DBusServiceProxy::Proxy::Proxy(DBus::Connection *connection)
    : DBus::ObjectProxy(*connection,
                        dbus::kDBusServicePath, dbus::kDBusServiceName) {}

DBusServiceProxy::Proxy::~Proxy() {}

void DBusServiceProxy::Proxy::set_name_owner_changed_callback(
    const NameOwnerChangedCallback &callback) {
  name_owner_changed_callback_ = callback;
}

void DBusServiceProxy::Proxy::NameOwnerChanged(const string &name,
                                               const string &old_owner,
                                               const string &new_owner) {
  if (!name_owner_changed_callback_.is_null()) {
    name_owner_changed_callback_.Run(name, old_owner, new_owner);
  }
}

void DBusServiceProxy::Proxy::GetNameOwnerCallback(const string &unique_name,
                                                   const DBus::Error &error,
                                                   void *data) {
  scoped_ptr<StringCallback> callback(reinterpret_cast<StringCallback *>(data));
  Error e;
  FromDBusError(error, &e);
  callback->Run(unique_name, e);
}

}  // namespace shill
