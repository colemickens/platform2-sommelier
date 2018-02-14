// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WIMAX_MANAGER_POWER_MANAGER_DBUS_PROXY_H_
#define WIMAX_MANAGER_POWER_MANAGER_DBUS_PROXY_H_

#include <stdint.h>

#include <vector>

#include <base/macros.h>

#include "wimax_manager/dbus_proxies/org.chromium.PowerManager.h"
#include "wimax_manager/dbus_proxy.h"

namespace wimax_manager {

class PowerManager;

class PowerManagerDBusProxy : public org::chromium::PowerManager_proxy,
                              public DBusProxy {
 public:
  PowerManagerDBusProxy(DBus::Connection* connection, PowerManager* manager);
  ~PowerManagerDBusProxy() override = default;

  void SuspendImminent(const std::vector<uint8_t>& serialized_proto) override;
  void SuspendDone(const std::vector<uint8_t>& serialized_proto) override;

 private:
  PowerManager* power_manager_;

  DISALLOW_COPY_AND_ASSIGN(PowerManagerDBusProxy);
};

}  // namespace wimax_manager

#endif  // WIMAX_MANAGER_POWER_MANAGER_DBUS_PROXY_H_
