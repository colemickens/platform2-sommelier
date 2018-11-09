// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APMANAGER_FIREWALL_PROXY_INTERFACE_H_
#define APMANAGER_FIREWALL_PROXY_INTERFACE_H_

#include <string>

namespace apmanager {

class FirewallProxyInterface {
 public:
  virtual ~FirewallProxyInterface() {}

  virtual bool RequestUdpPortAccess(const std::string& interface,
                                    uint16_t port) = 0;
  virtual bool ReleaseUdpPortAccess(const std::string& interface,
                                    uint16_t port) = 0;
};

}  // namespace apmanager

#endif  // APMANAGER_FIREWALL_PROXY_INTERFACE_H_
