// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "arc/network/manager.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdint.h>
#include <sys/prctl.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <utility>
#include <vector>

#include <base/bind.h>
#include <base/logging.h>
#include <base/message_loop/message_loop.h>
#include <base/posix/unix_domain_socket.h>
#include <base/strings/string_split.h>
#include <base/strings/string_util.h>
#include <base/strings/stringprintf.h>
#include <brillo/minijail/minijail.h>

#include "arc/network/guest_events.h"
#include "arc/network/ipc.pb.h"

namespace arc_networkd {

Manager::Manager(std::unique_ptr<HelperProcess> adb_proxy,
                 std::unique_ptr<HelperProcess> mcast_proxy,
                 std::unique_ptr<HelperProcess> nd_proxy,
                 bool enable_multinet)
    : adb_proxy_(std::move(adb_proxy)),
      mcast_proxy_(std::move(mcast_proxy)),
      nd_proxy_(std::move(nd_proxy)),
      addr_mgr_({
          AddressManager::Guest::ARC,
          AddressManager::Guest::ARC_NET,
      }),
      enable_multinet_(enable_multinet),
      gsock_(AF_UNIX, SOCK_DGRAM) {
  runner_ = std::make_unique<MinijailedProcessRunner>();
  datapath_ = std::make_unique<Datapath>(runner_.get());
}

int Manager::OnInit() {
  prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0);

  // Handle subprocess lifecycle.
  process_reaper_.Register(this);

  CHECK(process_reaper_.WatchForChild(
      FROM_HERE, adb_proxy_->pid(),
      base::Bind(&Manager::OnSubprocessExited, weak_factory_.GetWeakPtr(),
                 adb_proxy_->pid())))
      << "Failed to watch adb-proxy child process";
  CHECK(process_reaper_.WatchForChild(
      FROM_HERE, mcast_proxy_->pid(),
      base::Bind(&Manager::OnSubprocessExited, weak_factory_.GetWeakPtr(),
                 nd_proxy_->pid())))
      << "Failed to watch multicast-proxy child process";
  CHECK(process_reaper_.WatchForChild(
      FROM_HERE, nd_proxy_->pid(),
      base::Bind(&Manager::OnSubprocessExited, weak_factory_.GetWeakPtr(),
                 nd_proxy_->pid())))
      << "Failed to watch nd-proxy child process";

  // Setup the socket for guests to connect and notify certain events.
  // TODO(garrick): Remove once DBus API available.
  struct sockaddr_un addr = {0};
  socklen_t addrlen = 0;
  FillGuestSocketAddr(&addr, &addrlen);
  if (!gsock_.Bind((const struct sockaddr*)&addr, addrlen)) {
    LOG(ERROR) << "Cannot bind guest socket @" << kGuestSocketPath
               << "; exiting";
    return -1;
  }

  // TODO(garrick): Remove this workaround ASAP.
  // Handle signals for ARC lifecycle.
  RegisterHandler(SIGUSR1,
                  base::Bind(&Manager::OnSignal, base::Unretained(this)));
  RegisterHandler(SIGUSR2,
                  base::Bind(&Manager::OnSignal, base::Unretained(this)));
  gsock_watcher_ = base::FileDescriptorWatcher::WatchReadable(
      gsock_.fd(), base::BindRepeating(&Manager::OnFileCanReadWithoutBlocking,
                                       base::Unretained(this)));

  // Run after Daemon::OnInit().
  base::MessageLoopForIO::current()->task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&Manager::InitialSetup, weak_factory_.GetWeakPtr()));

  return DBusDaemon::OnInit();
}

void Manager::InitialSetup() {
  device_mgr_ = std::make_unique<DeviceManager>(
      std::make_unique<ShillClient>(std::move(bus_)), &addr_mgr_,
      datapath_.get(), !enable_multinet_, mcast_proxy_.get(), nd_proxy_.get());

  arc_svc_ = std::make_unique<ArcService>(device_mgr_.get(), datapath_.get(),
                                          !enable_multinet_);
  arc_svc_->RegisterMessageHandler(
      base::Bind(&Manager::OnGuestMessage, weak_factory_.GetWeakPtr()));

  nd_proxy_->Listen();
}

void Manager::OnFileCanReadWithoutBlocking() {
  char buf[128] = {0};
  std::vector<base::ScopedFD> fds{};
  ssize_t len =
      base::UnixDomainSocket::RecvMsg(gsock_.fd(), buf, sizeof(buf), &fds);

  if (len <= 0) {
    PLOG(WARNING) << "Read failed";
    return;
  }

  // Only expecting ARCVM start/stop events from Concierge here.
  auto event = ArcGuestEvent::Parse(buf);
  if (!event || !event->isVm()) {
    LOG(WARNING) << "Unexpected message received: " << buf;
    return;
  }

  GuestMessage msg;
  msg.set_type(GuestMessage::ARC_VM);
  msg.set_arcvm_vsock_cid(event->id());
  msg.set_event(event->isStarting() ? GuestMessage::START : GuestMessage::STOP);
  OnGuestMessage(msg);
}

// TODO(garrick): Remove this workaround ASAP.
bool Manager::OnSignal(const struct signalfd_siginfo& info) {
  // Only ARC++ scripts send signals so nothing to do for VM.
  (info.ssi_signo == SIGUSR1) ? arc_svc_->OnStart() : arc_svc_->OnStop();
  return false;
}

void Manager::OnShutdown(int* exit_code) {
  device_mgr_.reset();
}

void Manager::OnSubprocessExited(pid_t pid, const siginfo_t& info) {
  LOG(ERROR) << "Subprocess " << pid << " exited unexpectedly";
  Quit();
}

void Manager::OnGuestMessage(const GuestMessage& msg) {
  IpHelperMessage ipm;
  *ipm.mutable_guest_message() = msg;
  adb_proxy_->SendMessage(ipm);
}

}  // namespace arc_networkd
