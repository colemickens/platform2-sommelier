//
// Copyright (C) 2015 The Android Open Source Project
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
