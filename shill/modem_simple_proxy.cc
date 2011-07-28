// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/modem_simple_proxy.h"

using std::string;

namespace shill {

ModemSimpleProxy::ModemSimpleProxy(DBus::Connection *connection,
                                   const string &path,
                                   const string &service)
    : proxy_(connection, path, service) {}

ModemSimpleProxy::~ModemSimpleProxy() {}

DBusPropertiesMap ModemSimpleProxy::GetStatus() {
  return proxy_.GetStatus();
}

ModemSimpleProxy::Proxy::Proxy(DBus::Connection *connection,
                               const string &path,
                               const string &service)
    : DBus::ObjectProxy(*connection, path, service.c_str()) {}

ModemSimpleProxy::Proxy::~Proxy() {}

}  // namespace shill
