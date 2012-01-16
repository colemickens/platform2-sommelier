// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/modem_proxy.h"

#include <base/logging.h>

using std::string;

namespace shill {

ModemProxy::ModemProxy(ModemProxyDelegate *delegate,
                       DBus::Connection *connection,
                       const string &path,
                       const string &service)
    : proxy_(delegate, connection, path, service) {}

ModemProxy::~ModemProxy() {}

void ModemProxy::Enable(const bool enable) {
  proxy_.Enable(enable);
}

void ModemProxy::Disconnect() {
  proxy_.Disconnect();
}

ModemProxyInterface::Info ModemProxy::GetInfo() {
  return proxy_.GetInfo();
}

ModemProxy::Proxy::Proxy(ModemProxyDelegate *delegate,
                         DBus::Connection *connection,
                         const string &path,
                         const string &service)
    : DBus::ObjectProxy(*connection, path, service.c_str()),
      delegate_(delegate) {}

ModemProxy::Proxy::~Proxy() {}

void ModemProxy::Proxy::StateChanged(const uint32 &old,
                                     const uint32 &_new,
                                     const uint32 &reason) {
  VLOG(2) << __func__ << "(" << old << ", " << _new << ", " << reason << ")";
  delegate_->OnModemStateChanged(old, _new, reason);
}

}  // namespace shill
