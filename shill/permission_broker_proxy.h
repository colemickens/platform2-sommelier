// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_PERMISSION_BROKER_PROXY_H_
#define SHILL_PERMISSION_BROKER_PROXY_H_

#include <string>
#include <vector>

#include <base/macros.h>

#include "shill/dbus_proxies/org.chromium.PermissionBroker.h"
#include "shill/permission_broker_proxy_interface.h"

namespace DBus {
class Connection;
}  // namespace DBus

namespace shill {

class PermissionBrokerProxy : public PermissionBrokerProxyInterface {
 public:
  explicit PermissionBrokerProxy(DBus::Connection* connection);
  ~PermissionBrokerProxy() override;

  // Inherited from PermissionBrokerProxyInterface.
  bool RequestVpnSetup(const std::vector<std::string>& user_names,
                       const std::string& interface) override;

  bool RemoveVpnSetup() override;

 private:
  static const int kInvalidHandle;

  class Proxy : public org::chromium::PermissionBroker_proxy,
                public DBus::ObjectProxy {
   public:
    explicit Proxy(DBus::Connection* connection);
    ~Proxy() override;

   private:
    DISALLOW_COPY_AND_ASSIGN(Proxy);
  };

  Proxy proxy_;
  int lifeline_read_fd_;
  int lifeline_write_fd_;

  DISALLOW_COPY_AND_ASSIGN(PermissionBrokerProxy);
};

}  // namespace shill

#endif  // SHILL_PERMISSION_BROKER_PROXY_H_
