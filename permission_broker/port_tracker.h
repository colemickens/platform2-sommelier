// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PERMISSION_BROKER_PORT_TRACKER_H_
#define PERMISSION_BROKER_PORT_TRACKER_H_

#include <map>
#include <string>
#include <utility>
#include <vector>

#include <base/macros.h>
#include <base/message_loop/message_loop.h>
#include <base/sequenced_task_runner.h>

#include "permission_broker/firewall.h"

namespace permission_broker {

class PortTracker {
 public:
  typedef std::pair<uint16_t, std::string> Hole;

  explicit PortTracker(Firewall* firewall);
  virtual ~PortTracker();

  bool AllowTcpPortAccess(uint16_t port, const std::string& iface, int dbus_fd);
  bool AllowUdpPortAccess(uint16_t port, const std::string& iface, int dbus_fd);
  bool RevokeTcpPortAccess(uint16_t port, const std::string& iface);
  bool RevokeUdpPortAccess(uint16_t port, const std::string& iface);
  bool LockDownLoopbackTcpPort(uint16_t port, int dbus_fd);
  bool ReleaseLoopbackTcpPort(uint16_t port);

  // Close all outstanding firewall holes.
  void RevokeAllPortAccess();

  // Unblock all loopback ports.
  void UnblockLoopbackPorts();

 protected:
  PortTracker(scoped_refptr<base::SequencedTaskRunner> task_runner,
              Firewall* firewall);

 private:
  // Helper functions for process lifetime tracking.
  virtual int AddLifelineFd(int dbus_fd);
  virtual bool DeleteLifelineFd(int fd);
  virtual void CheckLifelineFds(bool reschedule_check);
  virtual void ScheduleLifelineCheck();

  bool PlugFirewallHole(int fd);

  // epoll(7) helper functions.
  virtual bool InitializeEpollOnce();

  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  int epfd_;

  // For each fd (process), keep track of which hole (port, interface)
  // it requested.
  std::map<int, Hole> tcp_holes_;
  std::map<int, Hole> udp_holes_;

  // For each hole (port, interface), keep track of which fd requested it.
  // We need this for Release{Tcp|Udp}Port(), to avoid traversing
  // |{tcp|udp}_holes_| each time.
  std::map<Hole, int> tcp_fds_;
  std::map<Hole, int> udp_fds_;

  // For each fd (process), keep track of which loopback port it requested.
  std::map<int, uint16_t> tcp_loopback_ports_;

  // For each loopback port, keep track of which fd requested it.
  // We need this for ReleaseLoopbackTcpPort() to avoid traversing
  // |tcp_loopback_ports_| each time.
  std::map<uint16_t, int> tcp_loopback_fds_;

  // |firewall_| is owned by the PermissionBroker object owning this instance
  // of PortTracker.
  Firewall* firewall_;

  DISALLOW_COPY_AND_ASSIGN(PortTracker);
};

}  // namespace permission_broker

#endif  // PERMISSION_BROKER_PORT_TRACKER_H_
