// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "arc/vm/vsock_proxy/client_proxy.h"

#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

// Needs to be included after sys/socket.h
#include <linux/un.h>
#include <linux/vm_sockets.h>

#include <utility>

#include <base/bind.h>
#include <base/logging.h>
#include <base/posix/eintr_wrapper.h>

#include "arc/vm/vsock_proxy/message.pb.h"
#include "arc/vm/vsock_proxy/vsock_proxy.h"

namespace arc {
namespace {

// Path to the socket file for ArcBridgeService.
constexpr char kSocketPath[] = "/var/run/chrome/arc_bridge.sock";

// Port for VSOCK.
constexpr unsigned int kVSockPort = 9999;

base::ScopedFD ConnectVSock() {
  LOG(INFO) << "Creating VSOCK...";
  struct sockaddr_vm sa = {};
  sa.svm_family = AF_VSOCK;
  sa.svm_cid = VMADDR_CID_HOST;
  sa.svm_port = kVSockPort;

  // TODO(hidehiko): Consider to time out.
  while (true) {
    base::ScopedFD fd(
        socket(AF_VSOCK, SOCK_STREAM | SOCK_CLOEXEC, 0 /* protocol */));
    if (!fd.is_valid()) {
      PLOG(ERROR) << "Failed to create VSOCK";
      return {};
    }

    LOG(INFO) << "Connecting VSOCK";
    if (HANDLE_EINTR(connect(fd.get(),
                             reinterpret_cast<const struct sockaddr*>(&sa),
                             sizeof(sa))) == -1) {
      fd.reset();
      PLOG(ERROR) << "Failed to connect. Waiting and then retry...";
      sleep(1);  // Arbitrary wait.
      continue;
    }

    LOG(INFO) << "VSOCK created.";
    return fd;
  }
}

// Creates a socket at kSocketPath, and starts listening.
base::ScopedFD CreateArcBridgeSocket() {
  LOG(INFO) << "Creating " << kSocketPath;
  base::ScopedFD fd(
      socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0 /* protocol */));
  if (!fd.is_valid()) {
    PLOG(ERROR) << "Failed to create unix domain socket";
    return {};
  }

  struct sockaddr_un sa = {};
  sa.sun_family = AF_UNIX;
  strncpy(sa.sun_path, kSocketPath, sizeof(sa.sun_path) - 1);

  // Remove stale file first. Ignore the error intentionally.
  unlink(kSocketPath);

  if (bind(fd.get(), reinterpret_cast<const struct sockaddr*>(&sa),
           sizeof(sa)) == -1) {
    PLOG(ERROR) << "Failed to bind a unix domain socket";
    return {};
  }

  if (fchmod(fd.get(), 0666) == -1) {
    PLOG(ERROR) << "Failed to set permission";
    return {};
  }

  if (listen(fd.get(), 5 /* backlog */) == -1) {
    PLOG(ERROR) << "Failed to start listening a socket";
    return {};
  }

  LOG(INFO) << kSocketPath << " created.";
  return fd;
}

base::ScopedFD AcceptSocket(int raw_fd) {
  base::ScopedFD fd(
      HANDLE_EINTR(accept4(raw_fd, nullptr, nullptr, SOCK_CLOEXEC)));
  if (!fd.is_valid()) {
    PLOG(ERROR) << "Failed to accept() unix domain socket";
    return {};
  }
  return fd;
}

}  // namespace

ClientProxy::ClientProxy() = default;

ClientProxy::~ClientProxy() = default;

void ClientProxy::Initialize() {
  // TODO(hidehiko): Rework on the initial socket creation protocol.
  // At the moment:
  // 1) Chrome creates a socket at /run/chrome/arc_bridge.sock (in host).
  // 2) Starts ARCVM, then start arcbridgeservice in the guest OS.
  // 3) ClientProxy in arcbridgeservice creates yet another socket at
  //    /var/run/chrome/arc_bridge.sock (in guest).
  // 4) ArcBridgeService in arcbridgeservice connects to
  //    /var/run/chrome/arc_bridge.sock, so
  //    ClientProxy::onLocalSocketReadReady() is called.
  // 5) Asynchronously, host side proxy should start, and listens VSOCK.
  // 6) ClientProxy connects to VSOCK (this is blocking operation.
  //    Waiting for 5)).
  // 7) Accepts the /var/run/chrome/arc_bridge.sock connection request
  //    (in guest). The handle is reserved with "1".
  // 8) In parallel with 6), host proxy connects to
  //    /run/chrome/arc_bridge.sock.
  //
  // Plan:
  // The main change is the connection order. Current protocol uses VSOCK
  // connection as a kind of a message to notify host that there is incoming
  // connection request to /var/run/chrome/arc_bridge.sock. Instead, in new
  // protocol, establish VSOCK at the beginning, and send an explicit
  // connection message when needed.
  // 1) Chrome creates a socket at /run/chrome/arc_bridge.sock (in host).
  // 2) Start ARCVM, then starts host proxy in host OS.
  // 3) Host proxy prepares VSOCK and listens it.
  // 4) ClientProxy (this class) in arcbridgeservice connects to VSOCK,
  //    and initializes VSockProxy.
  // 5) Creates /var/run/chrome/arc_bridge.sock in guest.
  // 6) ArcBridgeService in arcbridgeservice connects to the guest
  //    arc_bridge.sock.
  // 7) VSockProxy is notified, so send a message that a connection comes
  //    to host via VSOCK.
  // 8) Host proxy connects to /run/chrome/arc_bridge.sock, then
  //    ArcBridge connection between ARCVM and host is established.
  arc_bridge_socket_ = CreateArcBridgeSocket();
  LOG(INFO) << "Start observing " << kSocketPath;
  arc_bridge_socket_controller_ = base::FileDescriptorWatcher::WatchReadable(
      arc_bridge_socket_.get(),
      base::BindRepeating(&ClientProxy::OnLocalSocketReadReady,
                          weak_factory_.GetWeakPtr()));
}

void ClientProxy::OnLocalSocketReadReady() {
  LOG(INFO) << "Initial socket connection comes";
  arc_bridge_socket_controller_.reset();

  vsock_proxy_ =
      std::make_unique<VSockProxy>(VSockProxy::Type::CLIENT, ConnectVSock());
  // 1 is reserved for the initial socket handle.
  constexpr uint64_t kInitialSocketHandle = 1;
  vsock_proxy_->RegisterFileDescriptor(AcceptSocket(arc_bridge_socket_.get()),
                                       arc_proxy::FileDescriptor::SOCKET,
                                       kInitialSocketHandle);
  LOG(INFO) << "ClientProxy has started to work.";
}

}  // namespace arc
