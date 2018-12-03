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
#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/logging.h>
#include <base/posix/unix_domain_socket_linux.h>
#include <base/strings/string_number_conversions.h>
#include <base/strings/string_util.h>
#include <base/time/time.h>

namespace {

const int kContainerRetryDelaySeconds = 5;
const int kMaxContainerRetries = 60;
const char kContainerPIDPath[] =
    "/run/containers/android-run_oci/container.pid";

int GetContainerPID() {
  const base::FilePath path(kContainerPIDPath);
  std::string pid_str;
  if (!base::ReadFileToStringWithMaxSize(path, &pid_str, 16 /* max size */)) {
    LOG(ERROR) << "Failed to read pid file";
    return -1;
  }
  int pid;
  if (!base::StringToInt(base::TrimWhitespaceASCII(pid_str, base::TRIM_ALL),
                         &pid)) {
    LOG(ERROR) << "Failed to convert container pid string";
    return -1;
  }
  LOG(INFO) << "Read container pid as " << pid;
  return pid;
}

}  // namespace

namespace arc_networkd {

IpHelper::IpHelper(base::ScopedFD control_fd)
    : control_watcher_(FROM_HERE), con_pid_(0) {
  control_fd_ = std::move(control_fd);
}

int IpHelper::OnInit() {
  // Prevent the main process from sending us any signals.
  if (setsid() < 0) {
    PLOG(ERROR) << "Failed to created a new session with setsid: exiting";
    return -1;
  }

  // Handle container lifecycle.
  RegisterHandler(
      SIGUSR1, base::Bind(&IpHelper::OnContainerStart, base::Unretained(this)));
  RegisterHandler(
      SIGUSR2, base::Bind(&IpHelper::OnContainerStop, base::Unretained(this)));

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

  MessageLoopForIO::current()->WatchFileDescriptor(control_fd_.get(), true,
                                                   MessageLoopForIO::WATCH_READ,
                                                   &control_watcher_, this);
}

bool IpHelper::OnContainerStart(const struct signalfd_siginfo& info) {
  if (info.ssi_code == SI_USER) {
    LOG(INFO) << "Container starting";
    con_pid_ = GetContainerPID();
    CHECK_GT(con_pid_, 0);

    // Initialize the container interfaces.
    for (auto& config : arc_ip_configs_) {
      config.second->Init(con_pid_);
    }
  }

  // Stay registered.
  return false;
}

bool IpHelper::OnContainerStop(const struct signalfd_siginfo& info) {
  if (info.ssi_code == SI_USER) {
    LOG(INFO) << "Container stopping";
    con_pid_ = 0;
    con_init_tries_ = 0;

    // Reset the container interfaces.
    for (auto& config : arc_ip_configs_) {
      config.second->Init(0);
    }
  }

  // Stay registered.
  return false;
}

void IpHelper::AddDevice(const std::string& ifname,
                         const DeviceConfig& config) {
  LOG(INFO) << "Adding device " << ifname;
  auto device = std::make_unique<ArcIpConfig>(ifname, config);
  // If the container is already up, then this device needs to be initialized.
  if (con_pid_ > 0)
    device->Init(con_pid_);

  arc_ip_configs_.emplace(ifname, std::move(device));
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
    // control_watcher_.StopWatchingFileDescriptor();
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
  const std::string& dev_ifname = pending_command_.dev_ifname();
  const auto it = arc_ip_configs_.find(dev_ifname);
  if (it == arc_ip_configs_.end()) {
    if (pending_command_.has_dev_config()) {
      AddDevice(dev_ifname, pending_command_.dev_config());
    } else {
      LOG(ERROR) << "Unexpected device " << dev_ifname;
    }
    pending_command_.Clear();
    return;
  }

  auto* config = it->second.get();
  if (pending_command_.has_clear_arc_ip()) {
    config->Clear();
  } else if (pending_command_.has_set_arc_ip()) {
    const SetArcIp& ip = pending_command_.set_arc_ip();
    CHECK(ip.prefix_len() > 0 && ip.prefix_len() <= 128)
        << "Invalid prefix len " << ip.prefix_len();
    CHECK(ValidateIfname(ip.lan_ifname()))
        << "Invalid inbound iface name " << ip.lan_ifname();

    config->Set(ExtractAddr6(ip.prefix()), static_cast<int>(ip.prefix_len()),
                ExtractAddr6(ip.router()), ip.lan_ifname());
  } else if (pending_command_.has_enable_inbound_ifname()) {
    EnableInbound(dev_ifname, pending_command_.enable_inbound_ifname());
  } else if (pending_command_.has_disable_inbound()) {
    config->DisableInbound();
  } else if (pending_command_.has_teardown()) {
    arc_ip_configs_.erase(dev_ifname);
  }
  pending_command_.Clear();
}

void IpHelper::EnableInbound(const std::string& dev,
                             const std::string& ifname) {
  const auto it = arc_ip_configs_.find(dev);
  if (it == arc_ip_configs_.end()) {
    LOG(WARNING) << "Cannot enable " << dev << " missing";
    return;
  }
  // The container must be fully initialized and its side of the virtual
  // interface must be up before continuing.
  if (it->second->ContainerInit()) {
    it->second->EnableInbound(ifname);
  } else {
    if (++con_init_tries_ >= kMaxContainerRetries) {
      LOG(ERROR) << "Container failed to come up";
      Quit();
    }
    base::MessageLoopForIO::current()->task_runner()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&IpHelper::EnableInbound, weak_factory_.GetWeakPtr(), dev,
                   ifname),
        base::TimeDelta::FromSeconds(kContainerRetryDelaySeconds));
  }
}

}  // namespace arc_networkd
