// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_SHIMS_TASK_PROXY_H_
#define SHILL_SHIMS_TASK_PROXY_H_

#include <map>
#include <string>

#include <base/macros.h>
#include <shill/dbus_proxies/org.chromium.flimflam.Task.h>

namespace shill {

namespace shims {

class TaskProxy {
 public:
  TaskProxy(DBus::Connection* connection,
            const std::string& path,
            const std::string& service);
  ~TaskProxy();

  void Notify(const std::string& reason,
              const std::map<std::string, std::string>& dict);

  bool GetSecret(std::string* username, std::string* password);

 private:
  class Proxy : public org::chromium::flimflam::Task_proxy,
                public DBus::ObjectProxy {
   public:
    Proxy(DBus::Connection* connection,
          const std::string& path,
          const std::string& service);
    virtual ~Proxy();

   private:
    // Signal callbacks inherited from Task_proxy.
    // [None]

    // Method callbacks inherited from Task_proxy.
    // [None]

    DISALLOW_COPY_AND_ASSIGN(Proxy);
  };

  Proxy proxy_;

  DISALLOW_COPY_AND_ASSIGN(TaskProxy);
};

}  // namespace shims

}  // namespace shill

#endif  // SHILL_SHIMS_TASK_PROXY_H_
