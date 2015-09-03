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

#ifndef SHILL_RPC_TASK_DBUS_ADAPTOR_H_
#define SHILL_RPC_TASK_DBUS_ADAPTOR_H_

#include <map>
#include <string>

#include <base/macros.h>

#include "shill/adaptor_interfaces.h"
#include "shill/dbus_adaptor.h"
#include "shill/dbus_adaptors/org.chromium.flimflam.Task.h"

namespace shill {

class RPCTask;

// Subclass of DBusAdaptor for RPCTask objects. There is a 1:1 mapping between
// RPCTask and RPCTaskDBusAdaptor instances. Furthermore, the RPCTask owns the
// RPCTaskDBusAdaptor and manages its lifetime, so we're OK with
// RPCTaskDBusAdaptor having a bare pointer to its owner task.
class RPCTaskDBusAdaptor : public org::chromium::flimflam::Task_adaptor,
                           public DBusAdaptor,
                           public RPCTaskAdaptorInterface {
 public:
  static const char kPath[];

  RPCTaskDBusAdaptor(DBus::Connection* conn, RPCTask* task);
  ~RPCTaskDBusAdaptor() override;

  // Implementation of RPCTaskAdaptorInterface.
  const std::string& GetRpcIdentifier() override;
  const std::string& GetRpcConnectionIdentifier() override;

  // Implementation of Task_adaptor
  void getsec(std::string& user,
              std::string& password,
              DBus::Error& error) override;  // NOLINT
  void notify(const std::string& reason,
              const std::map<std::string, std::string>& dict,
              DBus::Error& error) override;  // NOLINT

 private:
  RPCTask* task_;
  const std::string connection_name_;

  DISALLOW_COPY_AND_ASSIGN(RPCTaskDBusAdaptor);
};

}  // namespace shill

#endif  // SHILL_RPC_TASK_DBUS_ADAPTOR_H_
