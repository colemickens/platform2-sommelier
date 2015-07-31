// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_PERMISSION_BROKER_PROXY_INTERFACE_H_
#define SHILL_PERMISSION_BROKER_PROXY_INTERFACE_H_

#include <string>
#include <vector>

namespace shill {

class PermissionBrokerProxyInterface {
 public:
  virtual ~PermissionBrokerProxyInterface() {}
  virtual bool RequestVpnSetup(const std::vector<std::string>& user_names,
                               const std::string& interface) = 0;
  virtual bool RemoveVpnSetup() = 0;
};

}  // namespace shill

#endif  // SHILL_PERMISSION_BROKER_PROXY_INTERFACE_H_
