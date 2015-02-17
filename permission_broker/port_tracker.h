// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PERMISSION_BROKER_PORT_TRACKER_H_
#define PERMISSION_BROKER_PORT_TRACKER_H_

#include <map>
#include <string>
#include <utility>

#include <base/macros.h>
#include <base/message_loop/message_loop.h>
#include <base/sequenced_task_runner.h>

#include "firewalld/dbus-proxies.h"

namespace permission_broker {

class PortTracker {
 public:
  typedef std::pair<uint16_t, std::string> Hole;

  explicit PortTracker(org::chromium::FirewalldProxyInterface* firewalld);
  virtual ~PortTracker();

  bool ProcessTcpPort(uint16_t port, const std::string& iface, int dbus_fd);
  bool ProcessUdpPort(uint16_t port, const std::string& iface, int dbus_fd);
  bool ReleaseTcpPort(uint16_t port, const std::string& iface);
  bool ReleaseUdpPort(uint16_t port, const std::string& iface);

 protected:
  PortTracker(scoped_refptr<base::SequencedTaskRunner> task_runner,
              org::chromium::FirewalldProxyInterface* firewalld);

 private:
  // Helper functions for process lifetime tracking.
  virtual int AddLifelineFd(int dbus_fd);
  virtual bool DeleteLifelineFd(int fd);
  virtual void CheckLifelineFds();
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

  // |firewalld_| is owned by the PermissionBroker object owning this instance
  // of PortTracker.
  org::chromium::FirewalldProxyInterface* firewalld_;

  DISALLOW_COPY_AND_ASSIGN(PortTracker);
};

}  // namespace permission_broker

#endif  // PERMISSION_BROKER_PORT_TRACKER_H_
