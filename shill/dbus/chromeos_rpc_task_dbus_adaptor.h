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

class RPCTask;

// Subclass of DBusAdaptor for RPCTask objects. There is a 1:1 mapping between
// RPCTask and ChromeosRPCTaskDBusAdaptor instances. Furthermore, the RPCTask
// owns the ChromeosRPCTaskDBusAdaptor and manages its lifetime, so we're OK
// with RPCTaskDBusAdaptor having a bare pointer to its owner task.
class ChromeosRPCTaskDBusAdaptor
    : public org::chromium::flimflam::TaskAdaptor,
      public org::chromium::flimflam::TaskInterface,
      public ChromeosDBusAdaptor,
      public RPCTaskAdaptorInterface {
 public:
  static const char kPath[];

  ChromeosRPCTaskDBusAdaptor(const scoped_refptr<dbus::Bus>& bus,
                             RPCTask* task);
  ~ChromeosRPCTaskDBusAdaptor() override;

  // Implementation of RPCTaskAdaptorInterface.
  const std::string& GetRpcIdentifier() const override;
  const std::string& GetRpcConnectionIdentifier() const override;

  // Implementation of TaskAdaptor
  bool getsec(brillo::ErrorPtr* error,
              std::string* user,
              std::string* password) override;
  bool notify(brillo::ErrorPtr* error,
              const std::string& reason,
              const std::map<std::string, std::string>& dict) override;

 private:
  RPCTask* task_;
  const std::string connection_name_;

  DISALLOW_COPY_AND_ASSIGN(ChromeosRPCTaskDBusAdaptor);
};

}  // namespace shill

#endif  // SHILL_DBUS_CHROMEOS_RPC_TASK_DBUS_ADAPTOR_H_
