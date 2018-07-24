// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/dbus_service_proxy.h"

#include <memory>

#include <base/strings/stringprintf.h>
#include <chromeos/dbus/service_constants.h>

#include "shill/dbus_async_call_helper.h"
#include "shill/error.h"
#include "shill/logging.h"

using std::string;
using std::unique_ptr;

namespace shill {

namespace {

const char* kErrorNameHasNoOwner = "org.freedesktop.DBus.Error.NameHasNoOwner";

}  // namespace

DBusServiceProxy::DBusServiceProxy(DBus::Connection* connection)
    : proxy_(connection) {}

DBusServiceProxy::~DBusServiceProxy() {}

void DBusServiceProxy::GetNameOwner(const string& name,
                                    Error* error,
                                    const StringCallback& callback,
                                    int timeout) {
  BeginAsyncDBusCall(base::StringPrintf("%s(%s)", __func__, name.c_str()),
                     proxy_, &Proxy::GetNameOwnerAsync, callback,
                     error, &FromDBusError, timeout, name);
}

void DBusServiceProxy::set_name_owner_changed_callback(
    const NameOwnerChangedCallback& callback) {
  proxy_.set_name_owner_changed_callback(callback);
}

// static
void DBusServiceProxy::FromDBusError(const DBus::Error& dbus_error,
                                     Error* error) {
  if (!error) {
    return;
  }
  if (!dbus_error.is_set()) {
    error->Reset();
    return;
  }
  if (strcmp(dbus_error.name(), kErrorNameHasNoOwner) == 0) {
    // TODO(pstew): It would be ideal to surface this error more widely if
    // the service continues to have no owner after the name-owner timeout,
    // in order to eliminate startup transients.  crbug.com/499924
    LOG(INFO) << dbus_error.what();
  } else {
    Error::PopulateAndLog(
        FROM_HERE, error, Error::kOperationFailed, dbus_error.what());
  }
}

DBusServiceProxy::Proxy::Proxy(DBus::Connection* connection)
    : DBus::ObjectProxy(*connection,
                        dbus::kDBusServicePath, dbus::kDBusServiceName) {}

DBusServiceProxy::Proxy::~Proxy() {}

void DBusServiceProxy::Proxy::set_name_owner_changed_callback(
    const NameOwnerChangedCallback& callback) {
  name_owner_changed_callback_ = callback;
}

void DBusServiceProxy::Proxy::NameOwnerChanged(const string& name,
                                               const string& old_owner,
                                               const string& new_owner) {
  if (!name_owner_changed_callback_.is_null()) {
    name_owner_changed_callback_.Run(name, old_owner, new_owner);
  }
}

void DBusServiceProxy::Proxy::GetNameOwnerCallback(const string& unique_name,
                                                   const DBus::Error& error,
                                                   void* data) {
  unique_ptr<StringCallback> callback(reinterpret_cast<StringCallback*>(data));
  Error e;
  FromDBusError(error, &e);
  callback->Run(unique_name, e);
}

}  // namespace shill
