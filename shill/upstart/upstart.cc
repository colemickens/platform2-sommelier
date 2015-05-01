// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/upstart/upstart.h"

#include "shill/upstart/upstart_proxy_interface.h"
#include "shill/proxy_factory.h"

namespace shill {

// static
const char Upstart::kShillDisconnectEvent[] = "shill-disconnected";
const char Upstart::kShillConnectEvent[] = "shill-connected";

Upstart::Upstart(ProxyFactory *proxy_factory)
    : upstart_proxy_(proxy_factory->CreateUpstartProxy()) {}

Upstart::~Upstart() {}

void Upstart::NotifyDisconnected() {
  upstart_proxy_->EmitEvent(kShillDisconnectEvent, {}, false);
}

void Upstart::NotifyConnected() {
  upstart_proxy_->EmitEvent(kShillConnectEvent, {}, false);
}

}  // namespace shill
