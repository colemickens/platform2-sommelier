// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/dhcpcd_proxy.h"

#include <base/logging.h>

#include "shill/dhcp_provider.h"

using std::map;
using std::string;
using std::vector;

namespace shill {

const char DHCPCDProxy::kDBusInterfaceName[] = "org.chromium.dhcpcd";
const char DHCPCDProxy::kDBusPath[] = "/org/chromium/dhcpcd";

DHCPCDListener::DHCPCDListener(DHCPProvider *provider,
                               DBus::Connection *connection)
    : DBus::InterfaceProxy(DHCPCDProxy::kDBusInterfaceName),
      DBus::ObjectProxy(*connection, DHCPCDProxy::kDBusPath),
      provider_(provider) {
  VLOG(2) << __func__;
  connect_signal(DHCPCDListener, Event, EventSignal);
  connect_signal(DHCPCDListener, StatusChanged, StatusChangedSignal);
}

void DHCPCDListener::EventSignal(const DBus::SignalMessage &signal) {
  VLOG(2) << __func__;
  DBus::MessageIter ri = signal.reader();
  unsigned int pid;
  ri >> pid;
  VLOG(2) << "sender(" << signal.sender() << ") pid(" << pid << ")";

  DHCPConfigRefPtr config = provider_->GetConfig(pid);
  if (!config.get()) {
    LOG(ERROR) << "Unknown DHCP client PID " << pid;
    return;
  }
  config->InitProxy(&conn(), signal.sender());

  string reason;
  ri >> reason;
  DHCPConfig::Configuration configuration;
  ri >> configuration;
  config->ProcessEventSignal(reason, configuration);
}

void DHCPCDListener::StatusChangedSignal(const DBus::SignalMessage &signal) {
  VLOG(2) << __func__;
  DBus::MessageIter ri = signal.reader();
  unsigned int pid;
  ri >> pid;
  VLOG(2) << "sender(" << signal.sender() << ") pid(" << pid << ")";

  // Accept StatusChanged signals just to get the sender address and create an
  // appropriate proxy for the PID/sender pair.
  DHCPConfigRefPtr config = provider_->GetConfig(pid);
  if (!config.get()) {
    LOG(ERROR) << "Unknown DHCP client PID " << pid;
    return;
  }
  config->InitProxy(&conn(), signal.sender());
}

DHCPCDProxy::DHCPCDProxy(DBus::Connection *connection,
                         const char *service)
    : DBus::ObjectProxy(*connection, kDBusPath, service) {
  VLOG(2) << "DHCPCDProxy(service=" << service << ").";

  // Don't catch signals directly in this proxy because they will be dispatched
  // to us by the DHCPCD listener.
  _signals.erase("Event");
  _signals.erase("StatusChanged");
}

void DHCPCDProxy::DoRebind(const string &interface) {
  Rebind(interface);
}

void DHCPCDProxy::Event(const uint32_t &pid,
                        const std::string &reason,
                        const DHCPConfig::Configuration &configuration) {
  NOTREACHED();
}

void DHCPCDProxy::StatusChanged(const uint32_t &pid,
                                const std::string &status) {
  NOTREACHED();
}

}  // namespace shill
