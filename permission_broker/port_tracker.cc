// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "permission_broker/port_tracker.h"

#include <sys/epoll.h>
#include <unistd.h>

#include <string>

#include <base/bind.h>
#include <base/logging.h>

namespace {
const int kMaxEvents = 10;
const int64 kLifelineIntervalSeconds = 5;
}

namespace permission_broker {

PortTracker::PortTracker(org::chromium::FirewalldProxyInterface* firewalld)
    : task_runner_{base::MessageLoopForIO::current()->task_runner()},
      epfd_{-1},
      firewalld_{firewalld} {}

PortTracker::~PortTracker() {
  if (epfd_ >= 0) {
    close(epfd_);
  }
}

bool PortTracker::ProcessTcpPort(uint16_t port, int dbus_fd) {
  // We use |lifeline_fd| to track the lifetime of the process requesting
  // port access.
  int lifeline_fd = AddLifelineFd(dbus_fd);
  if (lifeline_fd < 0) {
    LOG(ERROR) << "Tracking lifeline fd for TCP port " << port << " failed";
    return false;
  }

  // Track the port.
  tcp_ports_[lifeline_fd] = port;

  bool success;
  firewalld_->PunchTcpHole(port, &success, nullptr);
  if (!success) {
    // If we fail to punch the hole in the firewall, stop tracking the lifetime
    // of the process.
    LOG(ERROR) << "Failed to punch hole for TCP port " << port;
    DeleteLifelineFd(lifeline_fd);
    tcp_ports_.erase(lifeline_fd);
    return false;
  }
  return true;
}

bool PortTracker::ProcessUdpPort(uint16_t port, int dbus_fd) {
  // We use |lifeline_fd| to track the lifetime of the process requesting
  // port access.
  int lifeline_fd = AddLifelineFd(dbus_fd);
  if (lifeline_fd < 0) {
    LOG(ERROR) << "Tracking lifeline fd for UDP port " << port << " failed";
    return false;
  }

  // Track the port.
  udp_ports_[lifeline_fd] = port;

  bool success;
  firewalld_->PunchUdpHole(port, &success, nullptr);
  if (!success) {
    // If we fail to punch the hole in the firewall, stop tracking the lifetime
    // of the process.
    LOG(ERROR) << "Failed to punch hole for UDP port " << port;
    DeleteLifelineFd(lifeline_fd);
    udp_ports_.erase(lifeline_fd);
    return false;
  }
  return true;
}

int PortTracker::AddLifelineFd(int dbus_fd) {
  if (!InitializeEpollOnce()) {
    return -1;
  }
  int fd = dup(dbus_fd);

  struct epoll_event epevent;
  epevent.events =
      EPOLLIN | EPOLLET;  // EPOLLERR | EPOLLHUP are always waited for.
  epevent.data.fd = fd;
  LOG(INFO) << "Adding file descriptor " << fd << " to epoll instance";
  if (epoll_ctl(epfd_, EPOLL_CTL_ADD, fd, &epevent) != 0) {
    PLOG(ERROR) << "epoll_ctl(EPOLL_CTL_ADD)";
    return -1;
  }

  // If this is the first port request, start lifeline checks.
  if (tcp_ports_.size() + udp_ports_.size() == 0) {
    LOG(INFO) << "Starting lifeline checks";
    ScheduleLifelineCheck();
  }

  return fd;
}

bool PortTracker::DeleteLifelineFd(int fd) {
  if (epfd_ < 0) {
    LOG(ERROR) << "epoll instance not created";
    return false;
  }

  LOG(INFO) << "Deleting file descriptor " << fd << " from epoll instance";
  if (epoll_ctl(epfd_, EPOLL_CTL_DEL, fd, NULL) != 0) {
    PLOG(ERROR) << "epoll_ctl(EPOLL_CTL_DEL)";
    return false;
  }
  return true;
}

void PortTracker::CheckLifelineFds() {
  if (epfd_ < 0) {
    return;
  }
  struct epoll_event epevents[kMaxEvents];
  int nready = epoll_wait(epfd_, epevents, kMaxEvents, 0 /* do not block */);
  if (nready < 0) {
    PLOG(ERROR) << "epoll_wait(0)";
    return;
  }
  if (nready == 0) {
    ScheduleLifelineCheck();
    return;
  }

  for (size_t eidx = 0; eidx < (size_t)nready; eidx++) {
    uint32_t events = epevents[eidx].events;
    int fd = epevents[eidx].data.fd;
    if ((events & (EPOLLHUP | EPOLLERR))) {
      // The process that requested this port has died/exited,
      // so we need to plug the hole.
      PlugFirewallHole(fd);
      DeleteLifelineFd(fd);
    }
  }

  // If there's still processes to track, schedule lifeline checks.
  if (tcp_ports_.size() + udp_ports_.size() > 0) {
    ScheduleLifelineCheck();
  } else {
    LOG(INFO) << "Stopping lifeline checks";
  }
}

void PortTracker::ScheduleLifelineCheck() {
  task_runner_->PostDelayedTask(
      FROM_HERE,
      base::Bind(&PortTracker::CheckLifelineFds, base::Unretained(this)),
      base::TimeDelta::FromSeconds(kLifelineIntervalSeconds));
}

void PortTracker::PlugFirewallHole(int fd) {
  bool success = false;
  uint16_t port = 0;
  if (tcp_ports_.find(fd) != tcp_ports_.end()) {
    // It was a TCP port.
    port = tcp_ports_[fd];
    firewalld_->PlugTcpHole(port, &success, nullptr);
    tcp_ports_.erase(fd);
    if (!success) {
      LOG(ERROR) << "Failed to plug hole for TCP port " << port;
    }
  } else if (udp_ports_.find(fd) != udp_ports_.end()) {
    // It was a UDP port.
    port = udp_ports_[fd];
    firewalld_->PlugUdpHole(port, &success, nullptr);
    udp_ports_.erase(fd);
    if (!success) {
      LOG(ERROR) << "Failed to plug hole for UDP port " << port;
    }
  } else {
    LOG(ERROR) << "File descriptor " << fd << " was not being tracked";
  }
}

bool PortTracker::InitializeEpollOnce() {
  if (epfd_ < 0) {
    // |size| needs to be > 0, but is otherwise ignored.
    LOG(INFO) << "Creating epoll instance";
    epfd_ = epoll_create(1 /* size */);
    if (epfd_ < 0) {
      PLOG(ERROR) << "epoll_create()";
      return false;
    }
  }
  return true;
}

}  // namespace permission_broker
