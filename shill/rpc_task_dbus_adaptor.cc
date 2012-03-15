// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/rpc_task_dbus_adaptor.h"

#include <base/logging.h>

#include "shill/error.h"
#include "shill/rpc_task.h"

using std::map;
using std::string;

namespace shill {

// static
const char RPCTaskDBusAdaptor::kPath[] = "/task/";

RPCTaskDBusAdaptor::RPCTaskDBusAdaptor(DBus::Connection *conn, RPCTask *task)
    : DBusAdaptor(conn, kPath + task->UniqueName()),
      task_(task),
      interface_name_(SHILL_INTERFACE ".Task"),
      connection_name_(conn->unique_name()) {}

RPCTaskDBusAdaptor::~RPCTaskDBusAdaptor() {
  task_ = NULL;
}

const string &RPCTaskDBusAdaptor::GetRpcIdentifier() {
  return DBus::Object::path();
}

const string &RPCTaskDBusAdaptor::GetRpcInterfaceIdentifier() {
  // TODO(petkov): We should be able to return DBus::Interface::name() or simply
  // name() and avoid the need for the |interface_name_| data member. However,
  // that's non-trivial due to multiple inheritance (crosbug.com/27058).
  return interface_name_;
}

const string &RPCTaskDBusAdaptor::GetRpcConnectionIdentifier() {
  return connection_name_;
}

void RPCTaskDBusAdaptor::notify(const string &reason,
                                const map<string, string> &dict,
                                DBus::Error &/*error*/) {
  task_->Notify(reason, dict);
}

}  // namespace shill
