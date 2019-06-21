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
#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/logging.h>
#include <base/message_loop/message_loop.h>
#include <base/posix/unix_domain_socket_linux.h>
#include <base/strings/stringprintf.h>
#include <base/strings/string_number_conversions.h>
#include <base/strings/string_split.h>
#include <base/strings/string_util.h>
#include <brillo/minijail/minijail.h>

#include "arc/network/guest_events.h"
#include "arc/network/ipc.pb.h"

namespace {

constexpr uint64_t kManagerCapMask = CAP_TO_MASK(CAP_NET_RAW);
constexpr char kUnprivilegedUser[] = "arc-networkd";

// TODO(garrick): Remove this workaround ASAP.
int GetContainerPID() {
  const base::FilePath path("/run/containers/android-run_oci/container.pid");
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
      arc_pid_(-1),
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

// TODO(garrick): Remove this workaround ASAP.
bool Manager::OnSignal(const struct signalfd_siginfo& info) {
  // Only ARC++ scripts send signals so noting to do for VM.
  if (info.ssi_signo == SIGUSR1) {
    arc_pid_ = GetContainerPID();
    if (arc_pid_ > 0) {
      OnGuestEvent(ArcGuestEvent(false /* vm */, true /* start */, arc_pid_));
    }
  } else {
    if (arc_pid_ > 0) {
      OnGuestEvent(ArcGuestEvent(false /* vm */, false /* start */, arc_pid_));
    }
  }

  return false;
}

void Manager::OnGuestNotification(const std::string& notification) {
  if (auto event = ArcGuestEvent::Parse(notification)) {
    OnGuestEvent(*event.get());
  }
}

void Manager::OnGuestEvent(const ArcGuestEvent& event) {
  GuestMessage msg;

  if (event.isVm()) {
    msg.set_type(GuestMessage::ARC_VM);
    msg.set_arcvm_vsock_cid(event.id());
  } else {
    msg.set_type(enable_multinet_ ? GuestMessage::ARC
                                  : GuestMessage::ARC_LEGACY);
    msg.set_arc_pid(event.id());
  }

  if (event.isStarting()) {
    msg.set_event(GuestMessage::START);
    device_mgr_->OnGuestStart();
  } else {
    msg.set_event(GuestMessage::STOP);
    device_mgr_->OnGuestStop();
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
