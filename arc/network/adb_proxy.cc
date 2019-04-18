// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "arc/network/adb_proxy.h"

#include <sys/socket.h>
#include <sys/types.h>

#include <utility>

#include <base/bind.h>
#include <base/logging.h>
#include <base/strings/stringprintf.h>
#include <brillo/minijail/minijail.h>

namespace arc_networkd {
namespace {
constexpr uint16_t kTcpPort = 5555;
constexpr uint32_t kTcpAddr = 0x64735C02;  // 100.115.92.2
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

  src_ = std::make_unique<Socket>(AF_INET, SOCK_STREAM | SOCK_NONBLOCK);
  struct sockaddr_in addr = {0};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(kTcpPort);
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  if (!src_->Bind((const struct sockaddr*)&addr)) {
    LOG(ERROR) << "Cannot bind source socket; exiting";
    return -1;
  }

  if (!src_->Listen(kMaxConn)) {
    LOG(ERROR) << "Cannot listen on source socket; exiting";
    return -1;
  }

  RegisterHandler(SIGUSR1,
                  base::Bind(&AdbProxy::OnSignal, base::Unretained(this)));
  RegisterHandler(SIGUSR2,
                  base::Bind(&AdbProxy::OnSignal, base::Unretained(this)));

  base::MessageLoopForIO::current()->WatchFileDescriptor(
      src_->fd(), true, base::MessageLoopForIO::WATCH_READ, &src_watcher_,
      this);

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
    if ((*it)->IsValid())
      ++it;
    else
      it = fwd_.erase(it);
  }
}

std::unique_ptr<Socket> AdbProxy::Connect() const {
  // TODO(garrick): Add others.
  struct sockaddr_in addr_in;
  addr_in.sin_family = AF_INET;
  addr_in.sin_port = htons(kTcpPort);
  addr_in.sin_addr.s_addr = htonl(kTcpAddr);

  auto dst = std::make_unique<Socket>(AF_INET, SOCK_STREAM);
  return dst->Connect((const struct sockaddr*)&addr_in) ? std::move(dst)
                                                        : nullptr;
}

bool AdbProxy::OnSignal(const struct signalfd_siginfo& info) {
  // On guest down just ensure any existing connections are culled.
  if (info.ssi_signo == SIGUSR2)
    fwd_.clear();

  // Stay registered.
  return false;
}

}  // namespace arc_networkd
