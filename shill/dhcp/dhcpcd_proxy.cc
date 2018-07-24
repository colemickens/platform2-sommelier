// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/dhcp/dhcpcd_proxy.h"

#include <dbus/dbus.h>
#include <string.h>

#include <limits>

#include "shill/dhcp/dhcp_provider.h"
#include "shill/logging.h"

using std::map;
using std::string;

namespace shill {

namespace Logging {
static auto kModuleLogScope = ScopeLogger::kDHCP;
static string ObjectID(DHCPCDProxy* d) { return "(dhcpcd_proxy)"; }
}

const char DHCPCDProxy::kDBusInterfaceName[] = "org.chromium.dhcpcd";
const char DHCPCDProxy::kDBusPath[] = "/org/chromium/dhcpcd";

DHCPCDListener::DHCPCDListener(DBus::Connection* connection,
                               DHCPProvider* provider)
    : proxy_(connection, provider) {}

DHCPCDListener::Proxy::Proxy(DBus::Connection* connection,
                             DHCPProvider* provider)
    : DBus::InterfaceProxy(DHCPCDProxy::kDBusInterfaceName),
      DBus::ObjectProxy(*connection, DHCPCDProxy::kDBusPath),
      provider_(provider) {
  SLOG(DHCP, nullptr, 2) << __func__;
  connect_signal(DHCPCDListener::Proxy, Event, EventSignal);
  connect_signal(DHCPCDListener::Proxy, StatusChanged, StatusChangedSignal);
}

DHCPCDListener::Proxy::~Proxy() {}

void DHCPCDListener::Proxy::EventSignal(const DBus::SignalMessage& signal) {
  SLOG(DBus, nullptr, 2) << __func__;
  DBus::MessageIter ri = signal.reader();
  unsigned int pid = std::numeric_limits<unsigned int>::max();
  try {
    ri >> pid;
  } catch (const DBus::Error& e) {
    LOG(FATAL) << "DBus exception: " << e.name() << ": " << e.what()
               << " interface: " << signal.interface()
               << " member: " << signal.member() << " path: " << signal.path();
  }
  SLOG(DHCP, nullptr, 2) << "sender(" << signal.sender()
                         << ") pid(" << pid << ")";

  DHCPConfigRefPtr config = provider_->GetConfig(pid);
  if (!config.get()) {
    if (provider_->IsRecentlyUnbound(pid)) {
      SLOG(DHCP, nullptr, 3)
          << __func__ << ": ignoring message from recently unbound PID " << pid;
    } else {
      LOG(ERROR) << "Unknown DHCP client PID " << pid;
    }
    return;
  }
  config->InitProxy(signal.sender());

  string reason;
  try {
  ri >> reason;
  } catch (const DBus::Error& e) {
    LOG(FATAL) << "DBus exception: " << e.name() << ": " << e.what()
               << " interface: " << signal.interface()
               << " member: " << signal.member() << " path: " << signal.path();
  }
  map<string, DBus::Variant> configuration;
  try {
    ri >> configuration;
  } catch (const DBus::Error& e) {
    LOG(FATAL) << "DBus exception: " << e.name() << ": " << e.what()
               << " interface: " << signal.interface()
               << " member: " << signal.member() << " path: " << signal.path();
  }
  KeyValueStore configuration_store;
  Error error;
  DBusProperties::ConvertMapToKeyValueStore(configuration,
                                            &configuration_store,
                                            &error);
  if (error.IsFailure()) {
    LOG(ERROR) << "Failed to parse configuration properties";
    return;
  }
  config->ProcessEventSignal(reason, configuration_store);
}

void DHCPCDListener::Proxy::StatusChangedSignal(
    const DBus::SignalMessage& signal) {
  SLOG(DBus, nullptr, 2) << __func__;
  DBus::MessageIter ri = signal.reader();
  unsigned int pid = std::numeric_limits<unsigned int>::max();
  try {
    ri >> pid;
  } catch (const DBus::Error& e) {
    LOG(FATAL) << "DBus exception: " << e.name() << ": " << e.what()
               << " interface: " << signal.interface()
               << " member: " << signal.member() << " path: " << signal.path();
  }
  SLOG(DHCP, nullptr, 2) << "sender(" << signal.sender()
                         << ") pid(" << pid << ")";

  // Accept StatusChanged signals just to get the sender address and create an
  // appropriate proxy for the PID/sender pair.
  DHCPConfigRefPtr config = provider_->GetConfig(pid);
  if (!config.get()) {
    if (provider_->IsRecentlyUnbound(pid)) {
      SLOG(DHCP, nullptr, 3)
          << __func__ << ": ignoring message from recently unbound PID " << pid;
    } else {
      LOG(ERROR) << "Unknown DHCP client PID " << pid;
    }
    return;
  }
  config->InitProxy(signal.sender());

  string status;
  try {
    ri >> status;
  } catch (const DBus::Error& e) {
    LOG(FATAL) << "DBus exception: " << e.name() << ": " << e.what()
               << " interface: " << signal.interface()
               << " member: " << signal.member() << " path: " << signal.path();
  }
  config->ProcessStatusChangeSignal(status);
}

DHCPCDProxy::DHCPCDProxy(DBus::Connection* connection, const string& service)
    : proxy_(connection, service) {
  SLOG(this, 2) << "DHCPCDProxy(service=" << service << ").";
}

void DHCPCDProxy::Rebind(const string& interface) {
  SLOG(DBus, nullptr, 2) << __func__;
  try {
    proxy_.Rebind(interface);
  } catch (const DBus::Error& e) {
    LogDbusError(e, __func__, interface);
  }
}

void DHCPCDProxy::Release(const string& interface) {
  SLOG(DBus, nullptr, 2) << __func__;
  try {
    proxy_.Release(interface);
  } catch (const DBus::Error& e) {
    LogDbusError(e, __func__, interface);
  }
}

void DHCPCDProxy::LogDbusError(const DBus::Error& e, const string& method,
                               const string& interface) {
  if (!strcmp(e.name(), DBUS_ERROR_SERVICE_UNKNOWN) ||
      !strcmp(e.name(), DBUS_ERROR_NO_REPLY)) {
    LOG(INFO) << method << ": dhcpcd daemon appears to have exited.";
  } else {
    LOG(FATAL) << "DBus exception: " << method << ": "
               << e.name() << ": " << e.what() << " interface: " << interface;
  }
}

DHCPCDProxy::Proxy::Proxy(DBus::Connection* connection,
                          const string& service)
    : DBus::ObjectProxy(*connection, kDBusPath, service.c_str()) {
  // Don't catch signals directly in this proxy because they will be dispatched
  // to the client by the DHCPCD listener.
  _signals.erase("Event");
  _signals.erase("StatusChanged");
}

DHCPCDProxy::Proxy::~Proxy() {}

void DHCPCDProxy::Proxy::Event(
    const uint32_t& /*pid*/,
    const std::string& /*reason*/,
    const std::map<std::string, DBus::Variant>& /*configuration*/) {
  SLOG(DBus, nullptr, 2) << __func__;
  NOTREACHED();
}

void DHCPCDProxy::Proxy::StatusChanged(const uint32_t& /*pid*/,
                                       const std::string& /*status*/) {
  SLOG(DBus, nullptr, 2) << __func__;
  NOTREACHED();
}

}  // namespace shill
