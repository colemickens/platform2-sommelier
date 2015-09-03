//
// Copyright (C) 2012 The Android Open Source Project
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

#include "shill/supplicant/supplicant_bss_proxy.h"

#include <map>
#include <string>

#include <dbus-c++/dbus.h>

#include "shill/dbus_properties.h"
#include "shill/logging.h"
#include "shill/wifi/wifi_endpoint.h"

using std::map;
using std::string;

namespace shill {

namespace Logging {
static string ObjectID(SupplicantBSSProxy* s) {
  return "(supplicant_bss_proxy)";
}
}

SupplicantBSSProxy::SupplicantBSSProxy(
    WiFiEndpoint* wifi_endpoint,
    DBus::Connection* bus,
    const ::DBus::Path& object_path,
    const char* dbus_addr)
    : proxy_(wifi_endpoint, bus, object_path, dbus_addr) {}

SupplicantBSSProxy::~SupplicantBSSProxy() {}

// definitions for private class SupplicantBSSProxy::Proxy

SupplicantBSSProxy::Proxy::Proxy(
    WiFiEndpoint* wifi_endpoint, DBus::Connection* bus,
    const DBus::Path& dbus_path, const char* dbus_addr)
    : DBus::ObjectProxy(*bus, dbus_path, dbus_addr),
      wifi_endpoint_(wifi_endpoint) {}

SupplicantBSSProxy::Proxy::~Proxy() {}

void SupplicantBSSProxy::Proxy::PropertiesChanged(
    const std::map<string, ::DBus::Variant>& properties) {
  SLOG(DBus, nullptr, 2) << __func__;
  KeyValueStore properties_store;
  Error error;
  DBusProperties::ConvertMapToKeyValueStore(properties,
                                            &properties_store,
                                            &error);
  if (error.IsFailure()) {
    LOG(ERROR) << "Error encountered while parsing properties";
    return;
  }
  wifi_endpoint_->PropertiesChanged(properties_store);
}

}  // namespace shill
