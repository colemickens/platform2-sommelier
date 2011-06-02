// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/dhcpcd_proxy.h"

#include <base/logging.h>

namespace shill {

const char DHCPCDProxy::kDBusInterfaceName[] = "org.chromium.dhcpcd";
const char DHCPCDProxy::kDBusPath[] = "/org/chromium/dhcpcd";

DHCPCDListener::DHCPCDListener(DBus::Connection *connection)
    : DBus::InterfaceProxy(DHCPCDProxy::kDBusInterfaceName),
      DBus::ObjectProxy(*connection, DHCPCDProxy::kDBusPath) {
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
  // TODO(petkov): Dispatch the signal to the appropriate DHCPCDProxy.
}

void DHCPCDListener::StatusChangedSignal(const DBus::SignalMessage &signal) {
  VLOG(2) << __func__;
  DBus::MessageIter ri = signal.reader();
  unsigned int pid;
  ri >> pid;
  VLOG(2) << "sender(" << signal.sender() << ") pid(" << pid << ")";
  // TODO(petkov): Dispatch the signal to the appropriate DHCPCDProxy.
}

DHCPCDProxy::DHCPCDProxy(unsigned int pid,
                         DBus::Connection *connection,
                         const char *service)
    : DBus::ObjectProxy(*connection, kDBusPath, service),
      pid_(pid) {
  VLOG(2) << "DHCPCDListener(pid=" << pid_ << " service=" << service << ").";

  // Don't catch signals directly in this proxy because they will be dispatched
  // to us by the DHCPCD listener.
  _signals.erase("Event");
  _signals.erase("StatusChanged");
}

void DHCPCDProxy::Event(
    const uint32_t& pid,
    const std::string& reason,
    const std::map< std::string, DBus::Variant >& configuration) {
  VLOG(2) << "Event(pid=" << pid << " reason=\"" << reason << "\")";
  CHECK_EQ(pid, pid_);
}

void DHCPCDProxy::StatusChanged(const uint32_t& pid,
                                const std::string& status) {
  VLOG(2) << "StatusChanged(pid=" << pid << " status=\"" << status << "\")";
  CHECK_EQ(pid, pid_);
}

}  // namespace shill
