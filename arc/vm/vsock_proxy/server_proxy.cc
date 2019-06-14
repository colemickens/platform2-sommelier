// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "arc/vm/vsock_proxy/server_proxy.h"

#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

// Needs to be included after sys/socket.h
#include <linux/vm_sockets.h>

#include <utility>

#include <base/files/scoped_file.h>
#include <base/logging.h>
#include <base/posix/eintr_wrapper.h>

#include "arc/vm/vsock_proxy/file_descriptor_util.h"
#include "arc/vm/vsock_proxy/message.pb.h"
#include "arc/vm/vsock_proxy/vsock_proxy.h"

namespace arc {
namespace {

// Port for VSOCK.
constexpr unsigned int kVSockPort = 9999;

base::ScopedFD CreateVSock() {
  LOG(INFO) << "Creating VSOCK...";
  struct sockaddr_vm sa = {};
  sa.svm_family = AF_VSOCK;
  sa.svm_cid = VMADDR_CID_ANY;
  sa.svm_port = kVSockPort;

  base::ScopedFD fd(
      socket(AF_VSOCK, SOCK_STREAM | SOCK_CLOEXEC, 0 /* protocol */));
  if (!fd.is_valid()) {
    PLOG(ERROR) << "Failed to create VSOCK";
    return {};
  }

  if (bind(fd.get(), reinterpret_cast<const struct sockaddr*>(&sa),
           sizeof(sa)) == -1) {
    PLOG(ERROR) << "Failed to bind a unix domain socket";
    return {};
  }

  if (listen(fd.get(), 5 /* backlog */) == -1) {
    PLOG(ERROR) << "Failed to start listening a socket";
    return {};
  }

  LOG(INFO) << "VSOCK created.";
  return fd;
}

}  // namespace

ServerProxy::ServerProxy(ProxyFileSystem* proxy_file_system)
    : proxy_file_system_(proxy_file_system) {}

ServerProxy::~ServerProxy() = default;

bool ServerProxy::Initialize() {
  // The connection is established as follows.
  // 1) Chrome creates a socket at /run/chrome/arc_bridge.sock (in host).
  // 2) Start ARCVM, then starts host proxy in host OS.
  // 3) Host proxy prepares VSOCK and listens it.
  // 4) ClientProxy in arcbridgeservice connects to VSOCK, and initializes
  //    VSockProxy, then creates /var/run/chrome/arc_bridge.sock in guest.
  // 5) ArcBridgeService in arcbridgeservice connects to the guest
  //    arc_bridge.sock.
  // 6) VSockProxy in client is notified, so send a message to request connect
  //    to the /run/chrome/arc_bridge.sock to host via VSOCK.
  // 7) Host proxy connects as client requested, then returns its corresponding
  //    handle to client.
  // 8) Finally, ClientProxy accept(2)s the /var/run/chrome/arc_bridge.sock,
  //    and register the file descriptor with the returned handle.
  //    Now ArcBridge connection between ARCVM and host is established.
  auto vsock = CreateVSock();
  if (!vsock.is_valid())
    return false;

  LOG(INFO) << "Start observing VSOCK";
  auto accepted = AcceptSocket(vsock.get());
  if (!accepted.is_valid())
    return false;

  vsock.reset();
  LOG(INFO) << "Initial socket connection comes";
  vsock_proxy_ = std::make_unique<VSockProxy>(
      VSockProxy::Type::SERVER, proxy_file_system_, std::move(accepted));
  LOG(INFO) << "ServerProxy has started to work.";
  return true;
}

}  // namespace arc
