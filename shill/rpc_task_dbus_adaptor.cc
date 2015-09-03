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

#include "shill/rpc_task_dbus_adaptor.h"

#include "shill/error.h"
#include "shill/logging.h"
#include "shill/rpc_task.h"

using std::map;
using std::string;

namespace shill {

namespace Logging {
static auto kModuleLogScope = ScopeLogger::kDBus;
static string ObjectID(RPCTaskDBusAdaptor* r) { return r->GetRpcIdentifier(); }
}

// static
const char RPCTaskDBusAdaptor::kPath[] = "/task/";

RPCTaskDBusAdaptor::RPCTaskDBusAdaptor(DBus::Connection* conn, RPCTask* task)
    : DBusAdaptor(conn, kPath + task->UniqueName()),
      task_(task),
      connection_name_(conn->unique_name()) {}

RPCTaskDBusAdaptor::~RPCTaskDBusAdaptor() {
  task_ = nullptr;
}

const string& RPCTaskDBusAdaptor::GetRpcIdentifier() {
  return DBus::Object::path();
}

const string& RPCTaskDBusAdaptor::GetRpcConnectionIdentifier() {
  return connection_name_;
}

void RPCTaskDBusAdaptor::getsec(
    string& user, string& password, DBus::Error& error) {  // NOLINT
  SLOG(this, 2) << __func__ << ": " << user;
  task_->GetLogin(&user, &password);
}

void RPCTaskDBusAdaptor::notify(const string& reason,
                                const map<string, string>& dict,
                                DBus::Error& /*error*/) {  // NOLINT
  SLOG(this, 2) << __func__ << ": " << reason;
  task_->Notify(reason, dict);
}

}  // namespace shill
