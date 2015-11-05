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

#include "apmanager/dbus/dbus_control.h"

#include "apmanager/dbus/shill_dbus_proxy.h"

#if !defined(__ANDROID__)
#include "apmanager/dbus/permission_broker_dbus_proxy.h"
#else
#include "apmanager/dbus/firewalld_dbus_proxy.h"
#endif  //__ANDROID__

namespace apmanager {

DBusControl::DBusControl(const scoped_refptr<dbus::Bus>& bus)
    : bus_(bus) {}

DBusControl::~DBusControl() {}

std::unique_ptr<FirewallProxyInterface> DBusControl::CreateFirewallProxy(
    const base::Closure& service_appeared_callback,
    const base::Closure& service_vanished_callback) {
#if !defined(__ANDROID__)
  return std::unique_ptr<FirewallProxyInterface>(
      new PermissionBrokerDBusProxy(
          bus_, service_appeared_callback, service_vanished_callback));
#else
  return std::unique_ptr<FirewallProxyInterface>(
      new FirewalldDBusProxy(
          bus_, service_appeared_callback, service_vanished_callback));
#endif  // __ANDROID__
}

std::unique_ptr<ShillProxyInterface> DBusControl::CreateShillProxy(
    const base::Closure& service_appeared_callback,
    const base::Closure& service_vanished_callback) {
  return std::unique_ptr<ShillProxyInterface>(
      new ShillDBusProxy(
          bus_, service_appeared_callback, service_vanished_callback));
}

}  // namespace apmanager
