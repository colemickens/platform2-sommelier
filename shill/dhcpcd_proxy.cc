// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/dhcpcd_proxy.h"

#include <limits>

#include "shill/dhcp_provider.h"
#include "shill/logging.h"

using std::string;
using std::vector;

namespace shill {

const char DHCPCDProxy::kDBusInterfaceName[] = "org.chromium.dhcpcd";
const char DHCPCDProxy::kDBusPath[] = "/org/chromium/dhcpcd";

DHCPCDListener::DHCPCDListener(DBus::Connection *connection,
                               DHCPProvider *provider)
    : proxy_(connection, provider) {}

DHCPCDListener::Proxy::Proxy(DBus::Connection *connection,
                             DHCPProvider *provider)
    : DBus::InterfaceProxy(DHCPCDProxy::kDBusInterfaceName),
      DBus::ObjectProxy(*connection, DHCPCDProxy::kDBusPath),
      provider_(provider) {
  SLOG(DHCP, 2) << __func__;
  connect_signal(DHCPCDListener::Proxy, Event, EventSignal);
  connect_signal(DHCPCDListener::Proxy, StatusChanged, StatusChangedSignal);
}

void DHCPCDListener::Proxy::EventSignal(const DBus::SignalMessage &signal) {
  SLOG(DBus, 2) << __func__;
  DBus::MessageIter ri = signal.reader();
  unsigned int pid = std::numeric_limits<unsigned int>::max();
  try {
    ri >> pid;
  } catch (const DBus::Error &e) {
    LOG(FATAL) << "DBus exception: " << e.name() << ": " << e.what()
               << " interface: " << signal.interface()
               << " member: " << signal.member() << " path: " << signal.path();
  }
  SLOG(DHCP, 2) << "sender(" << signal.sender() << ") pid(" << pid << ")";

  DHCPConfigRefPtr config = provider_->GetConfig(pid);
  if (!config.get()) {
    LOG(ERROR) << "Unknown DHCP client PID " << pid;
    return;
  }
  config->InitProxy(signal.sender());

  string reason;
  try {
  ri >> reason;
  } catch (const DBus::Error &e) {
    LOG(FATAL) << "DBus exception: " << e.name() << ": " << e.what()
               << " interface: " << signal.interface()
               << " member: " << signal.member() << " path: " << signal.path();
  }
  DHCPConfig::Configuration configuration;
  try {
    ri >> configuration;
  } catch (const DBus::Error &e) {
    LOG(FATAL) << "DBus exception: " << e.name() << ": " << e.what()
               << " interface: " << signal.interface()
               << " member: " << signal.member() << " path: " << signal.path();
  }
  config->ProcessEventSignal(reason, configuration);
}

void DHCPCDListener::Proxy::StatusChangedSignal(
    const DBus::SignalMessage &signal) {
  SLOG(DBus, 2) << __func__;
  DBus::MessageIter ri = signal.reader();
  unsigned int pid = std::numeric_limits<unsigned int>::max();
  try {
    ri >> pid;
  } catch (const DBus::Error &e) {
    LOG(FATAL) << "DBus exception: " << e.name() << ": " << e.what()
               << " interface: " << signal.interface()
               << " member: " << signal.member() << " path: " << signal.path();
  }
  SLOG(DHCP, 2) << "sender(" << signal.sender() << ") pid(" << pid << ")";

  // Accept StatusChanged signals just to get the sender address and create an
  // appropriate proxy for the PID/sender pair.
  DHCPConfigRefPtr config = provider_->GetConfig(pid);
  if (!config.get()) {
    LOG(ERROR) << "Unknown DHCP client PID " << pid;
    return;
  }
  config->InitProxy(signal.sender());
}

DHCPCDProxy::DHCPCDProxy(DBus::Connection *connection, const string &service)
    : proxy_(connection, service) {
  SLOG(DHCP, 2) << "DHCPCDProxy(service=" << service << ").";
}

void DHCPCDProxy::Rebind(const string &interface) {
  SLOG(DBus, 2) << __func__;
  try {
    proxy_.Rebind(interface);
  } catch (const DBus::Error &e) {
    LOG(FATAL) << "DBus exception: " << e.name() << ": " << e.what()
               << " interface: " << interface;
  }
}

void DHCPCDProxy::Release(const string &interface) {
  SLOG(DBus, 2) << __func__;
  try {
    proxy_.Release(interface);
  } catch (const DBus::Error &e) {
    LOG(FATAL) << "DBus exception: " << e.name() << ": " << e.what()
               << " interface: " << interface;
  }
}

DHCPCDProxy::Proxy::Proxy(DBus::Connection *connection,
                          const string &service)
    : DBus::ObjectProxy(*connection, kDBusPath, service.c_str()) {
  // Don't catch signals directly in this proxy because they will be dispatched
  // to the client by the DHCPCD listener.
  _signals.erase("Event");
  _signals.erase("StatusChanged");
}

DHCPCDProxy::Proxy::~Proxy() {}

void DHCPCDProxy::Proxy::Event(
    const uint32_t &/*pid*/,
    const std::string &/*reason*/,
    const DHCPConfig::Configuration &/*configuration*/) {
  SLOG(DBus, 2) << __func__;
  NOTREACHED();
}

void DHCPCDProxy::Proxy::StatusChanged(const uint32_t &/*pid*/,
                                       const std::string &/*status*/) {
  SLOG(DBus, 2) << __func__;
  NOTREACHED();
}

}  // namespace shill
