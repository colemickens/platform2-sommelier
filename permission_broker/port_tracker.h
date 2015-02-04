// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PERMISSION_BROKER_PORT_TRACKER_H_
#define PERMISSION_BROKER_PORT_TRACKER_H_

#include <string>
#include <unordered_map>

#include <base/macros.h>
#include <base/message_loop/message_loop.h>
#include <base/sequenced_task_runner.h>

#include "firewalld/dbus-proxies.h"

namespace permission_broker {

class PortTracker {
 public:
  explicit PortTracker(org::chromium::FirewalldProxyInterface* firewalld);
  virtual ~PortTracker();

  bool ProcessTcpPort(uint16_t port, int dbus_fd);
  bool ProcessUdpPort(uint16_t port, int dbus_fd);

 private:
  // Helper functions for process lifetime tracking.
  virtual int AddLifelineFd(int dbus_fd);
  virtual bool DeleteLifelineFd(int fd);
  virtual void CheckLifelineFds();
  virtual void ScheduleLifelineCheck();

  void PlugFirewallHole(int fd);

  // epoll(7) helper functions.
  virtual bool InitializeEpollOnce();

  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  int epfd_;

  // For each fd (process), keep track of which port it requested.
  std::unordered_map<int, uint16_t> tcp_ports_;
  std::unordered_map<int, uint16_t> udp_ports_;

  // |firewalld_| is owned by the PermissionBroker object owning this instance
  // of PortTracker.
  org::chromium::FirewalldProxyInterface* firewalld_;

  DISALLOW_COPY_AND_ASSIGN(PortTracker);
};

}  // namespace permission_broker

#endif  // PERMISSION_BROKER_PORT_TRACKER_H_
