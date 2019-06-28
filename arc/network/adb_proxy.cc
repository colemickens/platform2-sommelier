// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "arc/network/adb_proxy.h"

#include <linux/vm_sockets.h>
#include <sys/socket.h>
#include <sys/types.h>

#include <utility>

#include <base/bind.h>
#include <base/logging.h>
#include <base/strings/stringprintf.h>
#include <brillo/minijail/minijail.h>

#include "arc/network/net_util.h"

namespace arc_networkd {
namespace {
// adb gets confused if we listen on 5555 and thinks there is an emulator
// running, which in turn ends up confusing our integration test libraries
// because multiple devices show up.
constexpr uint16_t kTcpListenPort = 5550;
// But we still connect to adbd on its standard TCP port.
constexpr uint16_t kTcpConnectPort = 5555;
constexpr uint32_t kTcpAddr = Ipv4Addr(100, 115, 92, 2);
constexpr uint32_t kVsockPort = 5555;
// Reference: (./src/private-overlays/project-cheets-private/
// chromeos-base/android-vm-pi/files/run-arcvm)
constexpr uint32_t kVsockCid = 5;
constexpr uint64_t kCapMask = CAP_TO_MASK(CAP_NET_RAW);
constexpr char kUnprivilegedUser[] = "arc-networkd";
constexpr int kMaxConn = 16;

}  // namespace

AdbProxy::AdbProxy(base::ScopedFD control_fd)
    : msg_dispatcher_(std::move(control_fd)),
      src_watcher_(FROM_HERE),
      arc_type_(GuestMessage::UNKNOWN_GUEST) {
  msg_dispatcher_.RegisterFailureHandler(
      base::Bind(&AdbProxy::OnParentProcessExit, weak_factory_.GetWeakPtr()));

  msg_dispatcher_.RegisterGuestMessageHandler(
      base::Bind(&AdbProxy::OnGuestMessage, weak_factory_.GetWeakPtr()));
}

AdbProxy::~AdbProxy() {
  src_watcher_.StopWatchingFileDescriptor();
}

int AdbProxy::OnInit() {
  // Prevent the main process from sending us any signals.
  if (setsid() < 0) {
    PLOG(ERROR) << "Failed to created a new session with setsid; exiting";
    return -1;
  }

  // Run with minimal privileges.
  brillo::Minijail* m = brillo::Minijail::GetInstance();
  struct minijail* jail = m->New();

  // Most of these return void, but DropRoot() can fail if the user/group
  // does not exist.
  CHECK(m->DropRoot(jail, kUnprivilegedUser, kUnprivilegedUser))
      << "Could not drop root privileges";
  m->UseCapabilities(jail, kCapMask);
  m->Enter(jail);
  m->Destroy(jail);

  RegisterHandler(SIGUSR1,
                  base::Bind(&AdbProxy::OnSignal, base::Unretained(this)));
  RegisterHandler(SIGUSR2,
                  base::Bind(&AdbProxy::OnSignal, base::Unretained(this)));
  return Daemon::OnInit();
}

void AdbProxy::Reset() {
  src_watcher_.StopWatchingFileDescriptor();
  src_.reset();
  fwd_.clear();
}

void AdbProxy::OnParentProcessExit() {
  LOG(ERROR) << "Quitting because the parent process died";
  Reset();
  Quit();
}

void AdbProxy::OnFileCanReadWithoutBlocking(int fd) {
  if (auto conn = src_->Accept()) {
    if (auto dst = Connect()) {
      LOG(INFO) << "Connection established: " << *conn << " <-> " << *dst;
      auto fwd = std::make_unique<SocketForwarder>(
          base::StringPrintf("adbp%d-%d", conn->fd(), dst->fd()),
          std::move(conn), std::move(dst));
      fwd->Start();
      fwd_.emplace_back(std::move(fwd));
    }
  }

  // Cleanup any defunct forwarders.
  for (auto it = fwd_.begin(); it != fwd_.end();) {
    if (!(*it)->IsRunning() && (*it)->HasBeenStarted())
      it = fwd_.erase(it);
    else
      ++it;
  }
}

std::unique_ptr<Socket> AdbProxy::Connect() const {
  switch (arc_type_) {
    case GuestMessage::ARC: {
      struct sockaddr_in addr_in = {0};
      addr_in.sin_family = AF_INET;
      addr_in.sin_port = htons(kTcpConnectPort);
      addr_in.sin_addr.s_addr = kTcpAddr;
      auto dst = std::make_unique<Socket>(AF_INET, SOCK_STREAM);
      return dst->Connect((const struct sockaddr*)&addr_in, sizeof(addr_in))
                 ? std::move(dst)
                 : nullptr;
    }
    case GuestMessage::ARC_VM: {
      struct sockaddr_vm addr_vm = {0};
      addr_vm.svm_family = AF_VSOCK;
      addr_vm.svm_port = kVsockPort;
      addr_vm.svm_cid = kVsockCid;
      auto dst = std::make_unique<Socket>(AF_VSOCK, SOCK_STREAM);
      return dst->Connect((const struct sockaddr*)&addr_vm, sizeof(addr_vm))
                 ? std::move(dst)
                 : nullptr;
    }
    default:
      LOG(DFATAL) << "Unexpected connect - no ARC guest";
      return nullptr;
  }
}

void AdbProxy::OnGuestMessage(const GuestMessage& msg) {
  if (msg.type() != GuestMessage::ARC && msg.type() != GuestMessage::ARC_VM)
    return;

  arc_type_ = msg.type();

  // On ARC up, start accepting connections.
  if (msg.event() == GuestMessage::START) {
    src_ = std::make_unique<Socket>(AF_INET, SOCK_STREAM | SOCK_NONBLOCK);
    // Need to set this to reuse the port on localhost.
    int on = 1;
    if (setsockopt(src_->fd(), SOL_SOCKET, SO_REUSEADDR, &on, sizeof(int)) <
        0) {
      PLOG(ERROR) << "setsockopt(SO_REUSEADDR) failed";
      return;
    }
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(kTcpListenPort);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (!src_->Bind((const struct sockaddr*)&addr, sizeof(addr))) {
      LOG(ERROR) << "Cannot bind source socket";
      return;
    }

    if (!src_->Listen(kMaxConn)) {
      LOG(ERROR) << "Cannot listen on source socket";
      return;
    }

    // Run the accept loop.
    LOG(INFO) << "Accepting connections...";
    base::MessageLoopForIO::current()->WatchFileDescriptor(
        src_->fd(), true, base::MessageLoopForIO::WATCH_READ, &src_watcher_,
        this);
    return;
  }

  // On ARC down, cull any open connections and stop listening.
  if (msg.event() == GuestMessage::STOP) {
    Reset();
  }
}

// TODO(garrick): Remove this workaround ASAP.
bool AdbProxy::OnSignal(const struct signalfd_siginfo& info) {
  // Do nothing. This is only here to prevent the process from crashing.
  return false;
}

}  // namespace arc_networkd
