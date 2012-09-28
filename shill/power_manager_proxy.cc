// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/power_manager_proxy.h"

#include <chromeos/dbus/service_constants.h>

#include "shill/logging.h"

using std::string;

namespace shill {

PowerManagerProxy::PowerManagerProxy(PowerManagerProxyDelegate *delegate,
                                     DBus::Connection *connection)
    : proxy_(delegate, connection) {}

PowerManagerProxy::~PowerManagerProxy() {}

void PowerManagerProxy::RegisterSuspendDelay(uint32 delay_ms) {
  LOG(INFO) << __func__ << "(" << delay_ms << ")";
  try {
    proxy_.RegisterSuspendDelay(delay_ms);
  } catch (const DBus::Error &e) {
    LOG(ERROR) << "DBus exception: " << e.name() << ": " << e.what()
               << "delay ms: " << delay_ms;
  }
}

void PowerManagerProxy::UnregisterSuspendDelay() {
  LOG(INFO) << __func__;
  try {
    proxy_.UnregisterSuspendDelay();
  } catch (const DBus::Error &e) {
    LOG(ERROR) << "DBus exception: " << e.name() << ": " << e.what();
  }
}

void PowerManagerProxy::SuspendReady(uint32 sequence_number) {
  // TODO(petkov): Fix this code after SuspendReady is converted to a method
  // call on the PowerManager DBus interface.
  LOG(INFO) << __func__  << "(" << sequence_number << ")";
  DBus::SignalMessage signal(power_manager::kPowerManagerServicePath,
                             power_manager::kPowerManagerInterface,
                             power_manager::kSuspendReady);
  DBus::MessageIter message = signal.writer();
  message << sequence_number;
  if (!proxy_.conn().send(signal)) {
    LOG(ERROR) << "Failed to signal suspend ready (" << sequence_number << ").";
  }
}

PowerManagerProxy::Proxy::Proxy(PowerManagerProxyDelegate *delegate,
                                DBus::Connection *connection)
    : DBus::ObjectProxy(*connection,
                        power_manager::kPowerManagerServicePath,
                        power_manager::kPowerManagerServiceName),
      delegate_(delegate) {}

PowerManagerProxy::Proxy::~Proxy() {}

void PowerManagerProxy::Proxy::SuspendDelay(const uint32_t &sequence_number) {
  LOG(INFO) << __func__ << "(" << sequence_number << ")";
  delegate_->OnSuspendDelay(sequence_number);
}

void PowerManagerProxy::Proxy::PowerStateChanged(
    const string &new_power_state) {
  LOG(INFO) << __func__ << "(" << new_power_state << ")";

  PowerManagerProxyDelegate::SuspendState suspend_state;
  if (new_power_state == "on") {
    suspend_state = PowerManagerProxyDelegate::kOn;
  } else if (new_power_state == "standby") {
    suspend_state = PowerManagerProxyDelegate::kStandby;
  } else if (new_power_state == "mem") {
    suspend_state = PowerManagerProxyDelegate::kMem;
  } else if (new_power_state == "disk") {
    suspend_state = PowerManagerProxyDelegate::kDisk;
  } else {
    suspend_state = PowerManagerProxyDelegate::kUnknown;
  }
  delegate_->OnPowerStateChanged(suspend_state);
}

}  // namespace shill
