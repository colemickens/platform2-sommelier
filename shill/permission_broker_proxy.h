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
