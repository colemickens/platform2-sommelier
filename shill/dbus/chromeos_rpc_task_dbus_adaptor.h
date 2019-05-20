// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_DBUS_CHROMEOS_RPC_TASK_DBUS_ADAPTOR_H_
#define SHILL_DBUS_CHROMEOS_RPC_TASK_DBUS_ADAPTOR_H_

#include <map>
#include <string>

#include <base/macros.h>

#include "dbus_bindings/org.chromium.flimflam.Task.h"
#include "shill/adaptor_interfaces.h"
#include "shill/dbus/chromeos_dbus_adaptor.h"

namespace shill {

class RpcTask;

// Subclass of DBusAdaptor for RpcTask objects. There is a 1:1 mapping between
// RpcTask and ChromeosRpcTaskDBusAdaptor instances. Furthermore, the RpcTask
// owns the ChromeosRpcTaskDBusAdaptor and manages its lifetime, so we're OK
// with RpcTaskDBusAdaptor having a bare pointer to its owner task.
class ChromeosRpcTaskDBusAdaptor
    : public org::chromium::flimflam::TaskAdaptor,
      public org::chromium::flimflam::TaskInterface,
      public ChromeosDBusAdaptor,
      public RpcTaskAdaptorInterface {
 public:
  static const char kPath[];

  ChromeosRpcTaskDBusAdaptor(const scoped_refptr<dbus::Bus>& bus,
                             RpcTask* task);
  ~ChromeosRpcTaskDBusAdaptor() override;

  // Implementation of RpcTaskAdaptorInterface.
  const RpcIdentifier& GetRpcIdentifier() const override;
  const RpcIdentifier& GetRpcConnectionIdentifier() const override;

  // Implementation of TaskAdaptor
  bool getsec(brillo::ErrorPtr* error,
              std::string* user,
              std::string* password) override;
  bool notify(brillo::ErrorPtr* error,
              const std::string& reason,
              const std::map<std::string, std::string>& dict) override;

 private:
  RpcTask* task_;
  const RpcIdentifier connection_name_;

  DISALLOW_COPY_AND_ASSIGN(ChromeosRpcTaskDBusAdaptor);
};

}  // namespace shill

#endif  // SHILL_DBUS_CHROMEOS_RPC_TASK_DBUS_ADAPTOR_H_
