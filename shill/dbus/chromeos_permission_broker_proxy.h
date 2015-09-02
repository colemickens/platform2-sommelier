// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_DBUS_CHROMEOS_PERMISSION_BROKER_PROXY_H_
#define SHILL_DBUS_CHROMEOS_PERMISSION_BROKER_PROXY_H_

#include <string>
#include <vector>

#include <base/macros.h>

#include "permission_broker/dbus-proxies.h"
#include "shill/firewall_proxy_interface.h"

namespace shill {

class ChromeosPermissionBrokerProxy : public FirewallProxyInterface {
 public:
  explicit ChromeosPermissionBrokerProxy(const scoped_refptr<dbus::Bus>& bus);
  ~ChromeosPermissionBrokerProxy() override;

  bool RequestVpnSetup(const std::vector<std::string>& user_names,
                       const std::string& interface) override;

  bool RemoveVpnSetup() override;

 private:
  static const int kInvalidHandle;

  std::unique_ptr<org::chromium::PermissionBrokerProxy> proxy_;
  int lifeline_read_fd_;
  int lifeline_write_fd_;

  base::WeakPtrFactory<ChromeosPermissionBrokerProxy> weak_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(ChromeosPermissionBrokerProxy);
};

}  // namespace shill

#endif  // SHILL_DBUS_CHROMEOS_PERMISSION_BROKER_PROXY_H_
