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

#ifndef SHILL_UPSTART_UPSTART_PROXY_H_
#define SHILL_UPSTART_UPSTART_PROXY_H_

// An implementation of UpstartProxyInterface.

#include <string>
#include <vector>

#include <base/compiler_specific.h>
#include <base/macros.h>

#include "shill/dbus_proxies/upstart.h"
#include "shill/upstart/upstart_proxy_interface.h"

namespace shill {

class Error;

class UpstartProxy : public UpstartProxyInterface {
 public:
  // Constructs a Upstart DBus object proxy with signals dispatched to
  // |delegate|.
  explicit UpstartProxy(DBus::Connection* connection);
  ~UpstartProxy() override = default;

  // Inherited from UpstartProxyInterface.
  void EmitEvent(const std::string& name,
                 const std::vector<std::string>& env,
                 bool wait) override;

 private:
  static const int kCommandTimeoutMilliseconds;

  class Proxy : public com::ubuntu::Upstart0_6_proxy,
                public DBus::ObjectProxy {
   public:
    explicit Proxy(DBus::Connection* connection);
    ~Proxy() override = default;

   private:
    static const char kServiceName[];
    static const char kServicePath[];

    // Signal callbacks inherited from org::chromium::Upstart0_6_proxy.
    void JobAdded(const ::DBus::Path& job) override;
    void JobRemoved(const ::DBus::Path& job) override;

    // Async callbacks.
    void EmitEventCallback(const ::DBus::Error& error, void* data) override;

    DISALLOW_COPY_AND_ASSIGN(Proxy);
  };

  // Dummy method required by async DBus call.
  static void FromDBusError(const DBus::Error& dbus_error, Error* error);

  Proxy proxy_;

  DISALLOW_COPY_AND_ASSIGN(UpstartProxy);
};

}  // namespace shill

#endif  // SHILL_UPSTART_UPSTART_PROXY_H_
