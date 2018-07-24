// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/upstart/upstart_proxy.h"

#include "shill/dbus_async_call_helper.h"

using std::string;
using std::vector;

namespace shill {

const int UpstartProxy::kCommandTimeoutMilliseconds = 2000;
const char UpstartProxy::Proxy::kServiceName[] = "com.ubuntu.Upstart";
const char UpstartProxy::Proxy::kServicePath[] = "/com/ubuntu/Upstart";

UpstartProxy::UpstartProxy(DBus::Connection* connection) : proxy_(connection) {}

void UpstartProxy::EmitEvent(
    const string& name, const vector<string>& env, bool wait) {
  BeginAsyncDBusCall(__func__, proxy_, &Proxy::EmitEventAsync, ResultCallback(),
                     nullptr, &FromDBusError, kCommandTimeoutMilliseconds,
                     name, env, wait);
}

// static
void UpstartProxy::FromDBusError(const DBus::Error& dbus_error, Error* error) {}

UpstartProxy::Proxy::Proxy(DBus::Connection* connection)
    : DBus::ObjectProxy(*connection,
                        kServicePath,
                        kServiceName) {}

// We care neither for the signals Upstart emits nor to replies to asynchronous
// method calls.
void UpstartProxy::Proxy::JobAdded(const ::DBus::Path& job) {}
void UpstartProxy::Proxy::JobRemoved(const ::DBus::Path& job) {}
void UpstartProxy::Proxy::EmitEventCallback(
    const ::DBus::Error& error, void* data) {}

}  // namespace shill
