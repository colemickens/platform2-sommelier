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

#ifndef SHILL_CELLULAR_MM1_BEARER_PROXY_H_
#define SHILL_CELLULAR_MM1_BEARER_PROXY_H_

#include <string>

#include "dbus_proxies/org.freedesktop.ModemManager1.Bearer.h"
#include "shill/cellular/mm1_bearer_proxy_interface.h"
#include "shill/dbus_properties.h"

namespace shill {
namespace mm1 {

// A proxy to org.freedesktop.ModemManager1.Bearer.
class BearerProxy : public BearerProxyInterface {
 public:
  // Constructs an org.freedesktop.ModemManager1.Bearer DBus object
  // proxy at |path| owned by |service|.
  BearerProxy(DBus::Connection* connection,
              const std::string& path,
              const std::string& service);

  ~BearerProxy() override;

  // Inherited methods from BearerProxyInterface
  void Connect(Error* error,
               const ResultCallback& callback,
               int timeout) override;
  void Disconnect(Error* error,
                  const ResultCallback& callback,
                  int timeout) override;

 private:
  class Proxy : public org::freedesktop::ModemManager1::Bearer_proxy,
                public DBus::ObjectProxy {
   public:
    Proxy(DBus::Connection* connection,
          const std::string& path,
          const std::string& service);
    ~Proxy() override;

   private:
    // Method callbacks inherited from
    // org::freedesktop::ModemManager1::BearerProxy
    void ConnectCallback(const ::DBus::Error& dberror,
                         void* data) override;
    void DisconnectCallback(const ::DBus::Error& dberror,
                            void* data) override;

    DISALLOW_COPY_AND_ASSIGN(Proxy);
  };

  Proxy proxy_;

  DISALLOW_COPY_AND_ASSIGN(BearerProxy);
};

}  // namespace mm1
}  // namespace shill

#endif  // SHILL_CELLULAR_MM1_BEARER_PROXY_H_
