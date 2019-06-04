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

AdbProxy::AdbProxy() : src_watcher_(FROM_HERE) {}

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
  // Try to connect with TCP IPv4.
  struct sockaddr_in addr_in = {0};
  addr_in.sin_family = AF_INET;
  addr_in.sin_port = htons(kTcpConnectPort);
  addr_in.sin_addr.s_addr = kTcpAddr;

  auto dst = std::make_unique<Socket>(AF_INET, SOCK_STREAM);
  if (dst->Connect((const struct sockaddr*)&addr_in, sizeof(addr_in)))
    return dst;

  // Try to connect with VSOCK.
  struct sockaddr_vm addr_vm = {0};
  addr_vm.svm_family = AF_VSOCK;
  addr_vm.svm_port = kVsockPort;
  addr_vm.svm_cid = kVsockCid;

  dst = std::make_unique<Socket>(AF_VSOCK, SOCK_STREAM);
  if (dst->Connect((const struct sockaddr*)&addr_vm, sizeof(addr_vm)))
    return dst;

  return nullptr;
}

bool AdbProxy::OnSignal(const struct signalfd_siginfo& info) {
  // On guest ARC up, start accepting connections.
  if (info.ssi_signo == SIGUSR1) {
    src_ = std::make_unique<Socket>(AF_INET, SOCK_STREAM | SOCK_NONBLOCK);
    // Need to set this to reuse the port on localhost.
    // TODO(garrick): Move this into Socket.
    int on = 1;
    if (setsockopt(src_->fd(), SOL_SOCKET, SO_REUSEADDR, &on, sizeof(int)) <
        0) {
      PLOG(ERROR) << "setsockopt(SO_REUSEADDR) failed";
      return false;
    }
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(kTcpListenPort);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (!src_->Bind((const struct sockaddr*)&addr, sizeof(addr))) {
      LOG(ERROR) << "Cannot bind source socket";
      return false;
    }

    if (!src_->Listen(kMaxConn)) {
      LOG(ERROR) << "Cannot listen on source socket";
      return false;
    }

    // Run the accept loop.
    base::MessageLoopForIO::current()->WatchFileDescriptor(
        src_->fd(), true, base::MessageLoopForIO::WATCH_READ, &src_watcher_,
        this);
  }

  // On ARC down cull any open connections and stop listening.
  if (info.ssi_signo == SIGUSR2) {
    src_watcher_.StopWatchingFileDescriptor();
    src_.reset();
    fwd_.clear();
  }

  // Stay registered.
  return false;
}

}  // namespace arc_networkd
