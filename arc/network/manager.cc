// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "arc/network/manager.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdint.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <utility>
#include <vector>

#include <base/bind.h>
#include <base/logging.h>
#include <base/message_loop/message_loop.h>
#include <base/posix/unix_domain_socket_linux.h>
#include <base/strings/stringprintf.h>
#include <base/strings/string_number_conversions.h>
#include <base/strings/string_split.h>
#include <brillo/minijail/minijail.h>

#include "arc/network/ipc.pb.h"

namespace {

constexpr uint64_t kManagerCapMask = CAP_TO_MASK(CAP_NET_RAW);
constexpr char kUnprivilegedUser[] = "arc-networkd";
constexpr char kGuestSocketPath[] = "/run/arc/network.gsock";
// These values are used to make guest event messages:
// Indicates the ARC guest (either ARC++ or ARCVM) is the source of the event.
constexpr char kGuestArc[] = "ARC";
// Indicates the guest is starting up.
constexpr char kEventUp[] = "UP";
// Indicates the guest is shutting down.
constexpr char kEventDown[] = "DOWN";

}  // namespace

namespace arc_networkd {

Manager::Manager(std::unique_ptr<HelperProcess> ip_helper,
                 std::unique_ptr<HelperProcess> adb_proxy,
                 bool enable_multinet)
    : ip_helper_(std::move(ip_helper)),
      adb_proxy_(std::move(adb_proxy)),
      addr_mgr_({
          AddressManager::Guest::ARC,
          AddressManager::Guest::ARC_NET,
      }),
      enable_multinet_(enable_multinet),
      gsock_(AF_UNIX, SOCK_DGRAM),
      gsock_watcher_(FROM_HERE) {}

Manager::~Manager() {
  gsock_watcher_.StopWatchingFileDescriptor();
}

int Manager::OnInit() {
  // Run with minimal privileges.
  brillo::Minijail* m = brillo::Minijail::GetInstance();
  struct minijail* jail = m->New();

  // Most of these return void, but DropRoot() can fail if the user/group
  // does not exist.
  CHECK(m->DropRoot(jail, kUnprivilegedUser, kUnprivilegedUser))
      << "Could not drop root privileges";
  m->UseCapabilities(jail, kManagerCapMask);
  m->Enter(jail);
  m->Destroy(jail);

  // Handle subprocess lifecycle.
  process_reaper_.Register(this);

  CHECK(process_reaper_.WatchForChild(
      FROM_HERE, ip_helper_->pid(),
      base::Bind(&Manager::OnSubprocessExited, weak_factory_.GetWeakPtr(),
                 ip_helper_->pid())))
      << "Failed to watch ip-helper child process";

  CHECK(process_reaper_.WatchForChild(
      FROM_HERE, adb_proxy_->pid(),
      base::Bind(&Manager::OnSubprocessExited, weak_factory_.GetWeakPtr(),
                 adb_proxy_->pid())))
      << "Failed to watch adb-proxy child process";

  // Setup the socket for guests to connect and notify certain events.
  struct sockaddr_un addr = {0};
  addr.sun_family = AF_UNIX;
  // Start at pos 1 to make this an abstract socket. Note that SUN_LEN does not
  // work in this case since it uses strlen, so this is the correct way to
  // compute the length of addr.
  strncpy(&addr.sun_path[1], kGuestSocketPath, strlen(kGuestSocketPath));
  socklen_t addrlen =
      offsetof(struct sockaddr_un, sun_path) + strlen(kGuestSocketPath) + 1;
  if (!gsock_.Bind((const struct sockaddr*)&addr, addrlen)) {
    LOG(ERROR) << "Cannot bind guest socket @" << kGuestSocketPath
               << "; exiting";
    return -1;
  }

  base::MessageLoopForIO::current()->WatchFileDescriptor(
      gsock_.fd(), true, base::MessageLoopForIO::WATCH_READ, &gsock_watcher_,
      this);

  // Run after Daemon::OnInit().
  base::MessageLoopForIO::current()->task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&Manager::InitialSetup, weak_factory_.GetWeakPtr()));

  return DBusDaemon::OnInit();
}

void Manager::InitialSetup() {
  device_mgr_ = std::make_unique<DeviceManager>(
      std::make_unique<ShillClient>(std::move(bus_)), &addr_mgr_,
      base::Bind(&Manager::SendDeviceMessage, weak_factory_.GetWeakPtr()),
      !enable_multinet_);
}

void Manager::OnFileCanReadWithoutBlocking(int fd) {
  char buf[128] = {0};
  std::vector<base::ScopedFD> fds{};
  ssize_t len = base::UnixDomainSocket::RecvMsg(fd, buf, sizeof(buf), &fds);

  if (len <= 0) {
    PLOG(WARNING) << "Read failed";
    return;
  }

  std::string msg(buf);
  OnGuestNotification(msg);
}

void Manager::OnGuestNotification(const std::string& notification) {
  // Notification strings are in the form:
  //   guest_type event_type [container_pid]
  // where the first two fields are required and the thrid is only for ARC++.
  const auto args = base::SplitString(notification, " ", base::TRIM_WHITESPACE,
                                      base::SPLIT_WANT_NONEMPTY);
  if (args.size() < 2) {
    LOG(WARNING) << "Unexpected notification: " << notification;
    return;
  }

  if (args[0] != kGuestArc) {
    LOG(DFATAL) << "Unexpected guest: " << args[0];
    return;
  }

  GuestMessage msg;
  msg.set_type(GuestMessage::ARC);

  if (args[1] == kEventUp) {
    msg.set_event(GuestMessage::START);
  } else if (args[1] == kEventDown) {
    msg.set_event(GuestMessage::STOP);
  } else {
    LOG(DFATAL) << "Unexpected event: " << args[1];
    return;
  }

  // ARC++
  if (args.size() == 3) {
    int pid = 0;
    if (!base::StringToInt(args[2], &pid)) {
      LOG(DFATAL) << "Invalid ARC++ pid: " << args[2];
      return;
    }
    msg.set_arc_pid(pid);

    if (msg.event() == GuestMessage::START) {
      device_mgr_->OnGuestStart();
    } else if (msg.event() == GuestMessage::STOP) {
      device_mgr_->OnGuestStop();
    }
  }

  SendGuestMessage(msg);
}

void Manager::OnShutdown(int* exit_code) {
  device_mgr_.reset();
}

void Manager::OnSubprocessExited(pid_t pid, const siginfo_t& info) {
  LOG(ERROR) << "Subprocess " << pid << " exited unexpectedly";
  Quit();
}

void Manager::SendGuestMessage(const GuestMessage& msg) {
  IpHelperMessage ipm;
  *ipm.mutable_guest_message() = msg;
  ip_helper_->SendMessage(ipm);
  adb_proxy_->SendMessage(ipm);
}

void Manager::SendDeviceMessage(const DeviceMessage& msg) {
  IpHelperMessage ipm;
  *ipm.mutable_device_message() = msg;
  ip_helper_->SendMessage(ipm);
}

}  // namespace arc_networkd
