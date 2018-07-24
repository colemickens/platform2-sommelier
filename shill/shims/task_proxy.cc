// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/shims/task_proxy.h"

#include <base/logging.h>

using std::map;
using std::string;

namespace shill {

namespace shims {

TaskProxy::TaskProxy(DBus::Connection* connection,
                     const string& path,
                     const string& service)
    : proxy_(connection, path, service) {}

TaskProxy::~TaskProxy() {}

void TaskProxy::Notify(const string& reason, const map<string, string>& dict) {
  LOG(INFO) << __func__ << "(" << reason
            << ", argcount: " << dict.size() << ")";
  try {
    proxy_.notify(reason, dict);
  } catch (const DBus::Error& e) {
    LOG(ERROR) << "DBus exception: " << e.name() << ": " << e.what();
  }
}

bool TaskProxy::GetSecret(string* username, string* password) {
  LOG(INFO) << __func__;
  try {
    proxy_.getsec(*username, *password);
  } catch (const DBus::Error& e) {
    LOG(ERROR) << "DBus exception: " << e.name() << ": " << e.what();
    return false;
  }
  return true;
}

TaskProxy::Proxy::Proxy(DBus::Connection* connection,
                        const std::string& path,
                        const std::string& service)
    : DBus::ObjectProxy(*connection, path, service.c_str()) {}

TaskProxy::Proxy::~Proxy() {}

}  // namespace shims

}  // namespace shill
