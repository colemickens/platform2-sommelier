// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/rpc_task.h"

#include <base/string_number_conversions.h>

#include "shill/adaptor_interfaces.h"
#include "shill/control_interface.h"
#include "shill/logging.h"

using std::map;
using std::string;

namespace shill {

// static
unsigned int RPCTask::serial_number_ = 0;

RPCTask::RPCTask(ControlInterface *control_interface, RPCTaskDelegate *delegate)
    : delegate_(delegate),
      unique_name_(base::UintToString(serial_number_++)),
      adaptor_(control_interface->CreateRPCTaskAdaptor(this)) {
  CHECK(delegate);
  SLOG(Task, 2) << "RPCTask " + unique_name_ + " created.";
}

RPCTask::~RPCTask() {
  SLOG(Task, 2) << "RPCTask " + unique_name_ + " destroyed.";
}

void RPCTask::GetLogin(string *user, string *password) {
  delegate_->GetLogin(user, password);
}

void RPCTask::Notify(const string &reason, const map<string, string> &dict) {
  delegate_->Notify(reason, dict);
}

string RPCTask::GetRpcIdentifier() const {
  return adaptor_->GetRpcIdentifier();
}

string RPCTask::GetRpcInterfaceIdentifier() const {
  return adaptor_->GetRpcInterfaceIdentifier();
}

string RPCTask::GetRpcConnectionIdentifier() const {
  return adaptor_->GetRpcConnectionIdentifier();
}

}  // namespace shill
