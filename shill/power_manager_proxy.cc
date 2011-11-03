// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/power_manager_proxy.h"

#include <base/logging.h>
#include <chromeos/dbus/service_constants.h>

namespace shill {

PowerManagerProxy::PowerManagerProxy(PowerManagerProxyDelegate *delegate,
                                     DBus::Connection *connection)
    : proxy_(delegate, connection) {}

PowerManagerProxy::~PowerManagerProxy() {}

void PowerManagerProxy::RegisterSuspendDelay(uint32 delay_ms) {
  proxy_.RegisterSuspendDelay(delay_ms);
}

PowerManagerProxy::Proxy::Proxy(PowerManagerProxyDelegate *delegate,
                                DBus::Connection *connection)
    : DBus::ObjectProxy(*connection,
                        power_manager::kPowerManagerServicePath,
                        power_manager::kPowerManagerServiceName),
      delegate_(delegate) {}

PowerManagerProxy::Proxy::~Proxy() {}

void PowerManagerProxy::Proxy::SuspendDelay(const uint32_t &sequence_number) {
  VLOG(2) << __func__ << "(" << sequence_number << ")";
  delegate_->OnSuspendDelay(sequence_number);
}

}  // namespace shill
