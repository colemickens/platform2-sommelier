// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "permission_broker/port_tracker.h"

#include <sys/epoll.h>
#include <unistd.h>

#include <string>
#include <vector>

#include <base/bind.h>
#include <base/logging.h>
#include <base/posix/eintr_wrapper.h>

namespace {
const int kMaxEvents = 10;
const int64_t kLifelineIntervalSeconds = 5;
const int kInvalidHandle = -1;
}  // namespace

namespace permission_broker {

PortTracker::PortTracker(Firewall* firewall)
    : task_runner_{base::MessageLoopForIO::current()->task_runner()},
      epfd_{kInvalidHandle},
      firewall_{firewall} {}

// Test-only.
PortTracker::PortTracker(scoped_refptr<base::SequencedTaskRunner> task_runner,
                         Firewall* firewall)
    : task_runner_{task_runner},
      epfd_{kInvalidHandle},
      firewall_{firewall} {}

PortTracker::~PortTracker() {
  RevokeAllPortAccess();
  UnblockLoopbackPorts();

  if (epfd_ >= 0) {
    close(epfd_);
  }
}

bool PortTracker::AllowTcpPortAccess(uint16_t port,
                                     const std::string& iface,
                                     int dbus_fd) {
  Hole hole = std::make_pair(port, iface);
  if (tcp_fds_.find(hole) != tcp_fds_.end()) {
    // This can happen when a requesting process has just been restarted but
    // the scheduled lifeline FD check hasn't yet been performed, so we might
    // have stale file descriptors around.
    // Force the FD check to see if they will be removed now.
    CheckLifelineFds(false);

    // Then try again. If this still fails, we know it's an invalid request.
    if (tcp_fds_.find(hole) != tcp_fds_.end()) {
      LOG(ERROR) << "Hole already punched for TCP port " << port;
      return false;
    }
  }

  // We use |lifeline_fd| to track the lifetime of the process requesting
  // port access.
  int lifeline_fd = AddLifelineFd(dbus_fd);
  if (lifeline_fd < 0) {
    LOG(ERROR) << "Tracking lifeline fd for TCP port " << port << " failed";
    return false;
  }

  // Track the hole.
  tcp_holes_[lifeline_fd] = hole;
  tcp_fds_[hole] = lifeline_fd;

  bool success = firewall_->AddAcceptRules(kProtocolTcp, port, iface);
  if (!success) {
    // If we fail to punch the hole in the firewall, stop tracking the lifetime
    // of the process.
    LOG(ERROR) << "Failed to punch hole for TCP port " << port;
    DeleteLifelineFd(lifeline_fd);
    tcp_holes_.erase(lifeline_fd);
    tcp_fds_.erase(hole);
    return false;
  }
  return true;
}

bool PortTracker::AllowUdpPortAccess(uint16_t port,
                                     const std::string& iface,
                                     int dbus_fd) {
  Hole hole = std::make_pair(port, iface);
  if (udp_fds_.find(hole) != udp_fds_.end()) {
    // This can happen when a requesting process has just been restarted but
    // the scheduled lifeline FD check hasn't yet been performed, so we might
    // have stale file descriptors around.
    // Force the FD check to see if they will be removed now.
    CheckLifelineFds(false);

    // Then try again. If this still fails, we know it's an invalid request.
    if (udp_fds_.find(hole) != udp_fds_.end()) {
      LOG(ERROR) << "Hole already punched for UDP port " << port;
      return false;
    }
  }

  // We use |lifeline_fd| to track the lifetime of the process requesting
  // port access.
  int lifeline_fd = AddLifelineFd(dbus_fd);
  if (lifeline_fd < 0) {
    LOG(ERROR) << "Tracking lifeline fd for UDP port " << port << " failed";
    return false;
  }

  // Track the hole.
  udp_holes_[lifeline_fd] = hole;
  udp_fds_[hole] = lifeline_fd;

  bool success = firewall_->AddAcceptRules(kProtocolUdp, port, iface);
  if (!success) {
    // If we fail to punch the hole in the firewall, stop tracking the lifetime
    // of the process.
    LOG(ERROR) << "Failed to punch hole for UDP port " << port;
    DeleteLifelineFd(lifeline_fd);
    udp_holes_.erase(lifeline_fd);
    udp_fds_.erase(hole);
    return false;
  }
  return true;
}

bool PortTracker::RevokeTcpPortAccess(uint16_t port, const std::string& iface) {
  Hole hole = std::make_pair(port, iface);
  auto p = tcp_fds_.find(hole);
  if (p == tcp_fds_.end()) {
    LOG(ERROR) << "Not tracking TCP port " << port << " on interface '" << iface
               << "'";
    return false;
  }

  int fd = p->second;
  bool plugged = PlugFirewallHole(fd);
  bool deleted = DeleteLifelineFd(fd);
  // PlugFirewallHole() prints an error message on failure,
  // but DeleteLifelineFd() does not, and even if it did,
  // we mock it out in tests.
  if (!deleted) {
    LOG(ERROR) << "Failed to delete file descriptor " << fd
               << " from epoll instance";
  }
  return plugged && deleted;
}

bool PortTracker::RevokeUdpPortAccess(uint16_t port, const std::string& iface) {
  Hole hole = std::make_pair(port, iface);
  auto p = udp_fds_.find(hole);
  if (p == udp_fds_.end()) {
    LOG(ERROR) << "Not tracking UDP port " << port << " on interface '" << iface
               << "'";
    return false;
  }

  int fd = p->second;
  bool plugged = PlugFirewallHole(fd);
  bool deleted = DeleteLifelineFd(fd);
  // PlugFirewallHole() prints an error message on failure,
  // but DeleteLifelineFd() does not, and even if it did,
  // we mock it out in tests.
  if (!deleted) {
    LOG(ERROR) << "Failed to delete file descriptor " << fd
               << " from epoll instance";
  }
  return plugged && deleted;
}

void PortTracker::RevokeAllPortAccess() {
  VLOG(1) << "Revoking all port access";

  // Copy the containers so that we can remove elements from the originals.
  // TCP
  auto holes = tcp_holes_;
  for (const auto& hole : holes) {
    int fd = hole.first;
    PlugFirewallHole(fd);
    DeleteLifelineFd(fd);
  }

  // UDP
  holes = udp_holes_;
  for (const auto& hole : holes) {
    int fd = hole.first;
    PlugFirewallHole(fd);
    DeleteLifelineFd(fd);
  }

  CHECK(tcp_holes_.size() == 0) << "Failed to plug all TCP holes";
  CHECK(udp_holes_.size() == 0) << "Failed to plug all UDP holes";
}

void PortTracker::UnblockLoopbackPorts() {
  VLOG(1) << "Unblocking all loopback ports";

  // Copy the containers so that we can remove elements from the originals.
  auto ports = tcp_loopback_ports_;
  for (const auto& pair : ports) {
    int fd = pair.first;
    PlugFirewallHole(fd);
    DeleteLifelineFd(fd);
  }

  CHECK(tcp_loopback_ports_.size() == 0)
      << "Failed to unblock all TCP loopback ports";
}

bool PortTracker::LockDownLoopbackTcpPort(uint16_t port, int dbus_fd) {
  if (tcp_loopback_fds_.find(port) != tcp_loopback_fds_.end()) {
    // This can happen when a requesting process has just been restarted but
    // the scheduled lifeline FD check hasn't yet been performed, so we might
    // have stale file descriptors around.
    // Force the FD check to see if they will be removed now.
    CheckLifelineFds(false);

    // Then try again. If this still fails, we know it's an invalid request.
    if (tcp_loopback_fds_.find(port) != tcp_loopback_fds_.end()) {
      LOG(ERROR) << "Loopback TCP port " << port << " already locked down";
      return false;
    }
  }

  // We use |lifeline_fd| to track the lifetime of the process requesting
  // port access.
  int lifeline_fd = AddLifelineFd(dbus_fd);
  if (lifeline_fd < 0) {
    LOG(ERROR) << "Tracking lifeline fd for loopback TCP port " << port
               << " failed";
    return false;
  }

  // Track the port.
  tcp_loopback_ports_[lifeline_fd] = port;
  tcp_loopback_fds_[port] = lifeline_fd;

  bool success = firewall_->AddLoopbackLockdownRules(kProtocolTcp, port);
  if (!success) {
    // If we fail to lock down the port in the firewall, stop tracking the
    // lifetime of the process.
    LOG(ERROR) << "Failed to lock down loopback TCP port " << port;
    DeleteLifelineFd(lifeline_fd);
    tcp_loopback_ports_.erase(lifeline_fd);
    tcp_loopback_fds_.erase(port);
    return false;
  }
  return true;
}

bool PortTracker::ReleaseLoopbackTcpPort(uint16_t port) {
  auto p = tcp_loopback_fds_.find(port);
  if (p == tcp_loopback_fds_.end()) {
    LOG(ERROR) << "Not tracking loopback TCP port " << port;
    return false;
  }

  int fd = p->second;
  bool plugged = PlugFirewallHole(fd);
  bool deleted = DeleteLifelineFd(fd);
  // PlugFirewallHole() prints an error message on failure,
  // but DeleteLifelineFd() does not, and even if it did,
  // we mock it out in tests.
  if (!deleted) {
    LOG(ERROR) << "Failed to delete file descriptor " << fd
               << " from epoll instance";
  }
  return plugged && deleted;
}

int PortTracker::AddLifelineFd(int dbus_fd) {
  if (!InitializeEpollOnce()) {
    return kInvalidHandle;
  }
  int fd = dup(dbus_fd);

  struct epoll_event epevent;
  epevent.events = EPOLLIN;  // EPOLLERR | EPOLLHUP are always waited for.
  epevent.data.fd = fd;
  VLOG(1) << "Adding file descriptor " << fd << " to epoll instance";
  if (epoll_ctl(epfd_, EPOLL_CTL_ADD, fd, &epevent) != 0) {
    PLOG(ERROR) << "epoll_ctl(EPOLL_CTL_ADD)";
    return kInvalidHandle;
  }

  // If this is the first port request, start lifeline checks.
  if (HasActiveRules()) {
    VLOG(1) << "Starting lifeline checks";
    ScheduleLifelineCheck();
  }

  return fd;
}

bool PortTracker::DeleteLifelineFd(int fd) {
  if (epfd_ < 0) {
    LOG(ERROR) << "epoll instance not created";
    return false;
  }

  VLOG(1) << "Deleting file descriptor " << fd << " from epoll instance";
  if (epoll_ctl(epfd_, EPOLL_CTL_DEL, fd, nullptr) != 0) {
    PLOG(ERROR) << "epoll_ctl(EPOLL_CTL_DEL)";
    return false;
  }

  // AddLifelineFd() calls dup(), so this function should close the fd.
  // We still return true since at this point the fd has been deleted from the
  // epoll instance.
  if (IGNORE_EINTR(close(fd)) < 0) {
    PLOG(ERROR) << "close(lifeline_fd)";
  }
  return true;
}

void PortTracker::CheckLifelineFds(bool reschedule_check) {
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
    if (reschedule_check)
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

  if (reschedule_check) {
    // If there's still processes to track, schedule lifeline checks.
    if (HasActiveRules()) {
      ScheduleLifelineCheck();
    } else {
      VLOG(1) << "Stopping lifeline checks";
    }
  }
}

void PortTracker::ScheduleLifelineCheck() {
  task_runner_->PostDelayedTask(
      FROM_HERE,
      base::Bind(&PortTracker::CheckLifelineFds, base::Unretained(this), true),
      base::TimeDelta::FromSeconds(kLifelineIntervalSeconds));
}

bool PortTracker::HasActiveRules() {
  return !tcp_holes_.empty() || !udp_holes_.empty() ||
         !tcp_loopback_ports_.empty();
}

bool PortTracker::PlugFirewallHole(int fd) {
  bool success = false;
  Hole hole;
  if (tcp_holes_.find(fd) != tcp_holes_.end()) {
    // It was a TCP hole.
    hole = tcp_holes_[fd];
    success = firewall_->DeleteAcceptRules(kProtocolTcp, hole.first /* port */,
                                          hole.second /* interface */);
    tcp_holes_.erase(fd);
    tcp_fds_.erase(hole);
    if (!success) {
      LOG(ERROR) << "Failed to plug hole for TCP port " << hole.first
                 << " on interface '" << hole.second << "'";
      return false;
    }
  } else if (udp_holes_.find(fd) != udp_holes_.end()) {
    // It was a UDP hole.
    hole = udp_holes_[fd];
    success = firewall_->DeleteAcceptRules(kProtocolUdp, hole.first /* port */,
                                          hole.second /* interface */);
    udp_holes_.erase(fd);
    udp_fds_.erase(hole);
    if (!success) {
      LOG(ERROR) << "Failed to plug hole for UDP port " << hole.first
                 << " on interface '" << hole.second << "'";
      return false;
    }
  } else if (tcp_loopback_ports_.find(fd) != tcp_loopback_ports_.end()) {
    // It was a blocked TCP loopback port.
    uint16_t port = tcp_loopback_ports_[fd];
    success = firewall_->DeleteLoopbackLockdownRules(kProtocolTcp, port);
    tcp_loopback_ports_.erase(fd);
    tcp_loopback_fds_.erase(port);
    if (!success) {
      LOG(ERROR) << "Failed to delete loopback lockdown rule for TCP port "
                 << port;
      return false;
    }
  } else {
    LOG(ERROR) << "File descriptor " << fd << " was not being tracked";
    return false;
  }
  return true;
}

bool PortTracker::InitializeEpollOnce() {
  if (epfd_ < 0) {
    // |size| needs to be > 0, but is otherwise ignored.
    VLOG(1) << "Creating epoll instance";
    epfd_ = epoll_create(1 /* size */);
    if (epfd_ < 0) {
      PLOG(ERROR) << "epoll_create()";
      return false;
    }
  }
  return true;
}

}  // namespace permission_broker
