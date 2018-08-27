// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "arc/network/ip_helper.h"

#include <ctype.h>
#include <net/if.h>
#include <unistd.h>

#include <utility>
#include <vector>

#include <base/bind.h>
#include <base/logging.h>
#include <base/posix/unix_domain_socket_linux.h>
#include <base/time/time.h>

namespace {

const int kContainerRetryDelaySeconds = 5;
const int kMaxContainerRetries = 60;

}  // namespace

namespace arc_networkd {

IpHelper::IpHelper(const Options& opt, base::ScopedFD control_fd)
    : control_watcher_(FROM_HERE) {
  arc_ip_config_.reset(
      new ArcIpConfig(opt.int_ifname, opt.con_ifname, opt.con_netns));
  control_fd_ = std::move(control_fd);
}

int IpHelper::OnInit() {
  VLOG(1) << "OnInit for " << *arc_ip_config_;

  // Prevent the main process from sending us any signals.
  if (setsid() < 0) {
    PLOG(ERROR) << "Failed to created a new session with setsid: exiting";
    return -1;
  }

  if (!arc_ip_config_->Init()) {
    LOG(ERROR) << "Error initializing arc_ip_config_";
    return -1;
  }

  // This needs to execute after Daemon::OnInit().
  base::MessageLoopForIO::current()->task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&IpHelper::InitialSetup, weak_factory_.GetWeakPtr()));

  return Daemon::OnInit();
}

void IpHelper::InitialSetup() {
  // Ensure that the parent is alive before trying to continue the setup.
  char buffer = 0;
  if (!base::UnixDomainSocket::SendMsg(control_fd_.get(), &buffer,
                                       sizeof(buffer), std::vector<int>())) {
    LOG(ERROR) << "Aborting setup flow because the parent died";
    Quit();
    return;
  }

  if (!arc_ip_config_->ContainerInit()) {
    if (++con_init_tries_ >= kMaxContainerRetries) {
      LOG(ERROR) << "Container failed to come up";
      Quit();
    } else {
      base::MessageLoopForIO::current()->task_runner()->PostDelayedTask(
          FROM_HERE,
          base::Bind(&IpHelper::InitialSetup, weak_factory_.GetWeakPtr()),
          base::TimeDelta::FromSeconds(kContainerRetryDelaySeconds));
    }
    return;
  }

  MessageLoopForIO::current()->WatchFileDescriptor(control_fd_.get(), true,
                                                   MessageLoopForIO::WATCH_READ,
                                                   &control_watcher_, this);
}

void IpHelper::OnFileCanReadWithoutBlocking(int fd) {
  CHECK_EQ(fd, control_fd_.get());

  char buffer[1024];
  std::vector<base::ScopedFD> fds{};
  ssize_t len =
      base::UnixDomainSocket::RecvMsg(fd, buffer, sizeof(buffer), &fds);

  // Exit whenever read fails or fd is closed.
  if (len <= 0) {
    PLOG(WARNING) << "Read failed: exiting";
    // The other side closed the connection.
    control_watcher_.StopWatchingFileDescriptor();
    Quit();
    return;
  }

  if (!pending_command_.ParseFromArray(buffer, len)) {
    LOG(ERROR) << "Error parsing protobuf";
    return;
  }
  HandleCommand();
}

const struct in6_addr& IpHelper::ExtractAddr6(const std::string& in) {
  CHECK_EQ(in.size(), sizeof(struct in6_addr));
  return *reinterpret_cast<const struct in6_addr*>(in.data());
}

bool IpHelper::ValidateIfname(const std::string& in) {
  if (in.size() >= IFNAMSIZ) {
    return false;
  }
  for (const char& c : in) {
    if (!isalnum(c) && c != '_') {
      return false;
    }
  }
  return true;
}

void IpHelper::HandleCommand() {
  if (pending_command_.has_clear_arc_ip()) {
    arc_ip_config_->Clear();
  } else if (pending_command_.has_set_arc_ip()) {
    const SetArcIp& ip = pending_command_.set_arc_ip();

    CHECK(ip.prefix_len() > 0 && ip.prefix_len() <= 128)
        << "Invalid prefix len " << ip.prefix_len();
    CHECK(ValidateIfname(ip.lan_ifname()))
        << "Invalid inbound iface name " << ip.lan_ifname();

    arc_ip_config_->Set(ExtractAddr6(ip.prefix()),
                        static_cast<int>(ip.prefix_len()),
                        ExtractAddr6(ip.router()), ip.lan_ifname());
  } else if (pending_command_.has_enable_inbound()) {
    std::string lan_ifname = pending_command_.enable_inbound();
    CHECK(ValidateIfname(lan_ifname))
        << "Invalid inbound iface name " << lan_ifname;
    arc_ip_config_->EnableInbound(pending_command_.enable_inbound());
  } else if (pending_command_.has_disable_inbound()) {
    arc_ip_config_->DisableInbound();
  }

  pending_command_.Clear();
}

}  // namespace arc_networkd
