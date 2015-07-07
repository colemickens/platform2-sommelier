// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
