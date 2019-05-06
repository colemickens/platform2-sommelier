// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/shims/task_proxy.h"

#include <base/logging.h>

using std::map;
using std::string;

namespace shill {

namespace shims {

TaskProxy::TaskProxy(scoped_refptr<dbus::Bus> bus,
                     const string& path,
                     const string& service)
      : proxy_(bus, dbus::ObjectPath(path)) {}

TaskProxy::~TaskProxy() = default;

void TaskProxy::Notify(const string& reason, const map<string, string>& dict) {
  LOG(INFO) << __func__ << "(" << reason
            << ", argcount: " << dict.size() << ")";
  brillo::ErrorPtr error;
  if (!proxy_.notify(reason, dict, &error)) {
    LOG(ERROR) << "DBus error: " << error->GetCode() << ": "
               << error->GetMessage();
  }
}

bool TaskProxy::GetSecret(string* username, string* password) {
  LOG(INFO) << __func__;
  brillo::ErrorPtr error;
  if (!proxy_.getsec(username, password, &error)) {
    LOG(ERROR) << "DBus error: " << error->GetCode() << ": "
               << error->GetMessage();
    return false;
  }
  return true;
}

}  // namespace shims

}  // namespace shill
