// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "arc/vm/vsock_proxy/client_proxy.h"

#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

// Needs to be included after sys/socket.h
#include <linux/vm_sockets.h>

#include <utility>

#include <base/bind.h>
#include <base/files/file_path.h>
#include <base/logging.h>
#include <base/posix/eintr_wrapper.h>

#include "arc/vm/vsock_proxy/file_descriptor_util.h"
#include "arc/vm/vsock_proxy/message.pb.h"
#include "arc/vm/vsock_proxy/vsock_proxy.h"

namespace arc {
namespace {

// Path to the socket file for ArcBridgeService.
constexpr char kGuestSocketPath[] = "/var/run/chrome/arc_bridge.sock";

// Path to the socket file for ArcBridgeService in host.
constexpr char kHostSocketPath[] = "/run/chrome/arc_bridge.sock";

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

}  // namespace

ClientProxy::ClientProxy() = default;

ClientProxy::~ClientProxy() = default;

void ClientProxy::Initialize() {
  // For the details of connection procedure, please find the comment in
  // ServerProxy::Initialize().
  vsock_proxy_ = std::make_unique<VSockProxy>(VSockProxy::Type::CLIENT, nullptr,
                                              ConnectVSock());

  arc_bridge_socket_ = CreateUnixDomainSocket(base::FilePath(kGuestSocketPath));
  LOG(INFO) << "Start observing " << kGuestSocketPath;
  arc_bridge_socket_controller_ = base::FileDescriptorWatcher::WatchReadable(
      arc_bridge_socket_.get(),
      base::BindRepeating(&ClientProxy::OnLocalSocketReadReady,
                          weak_factory_.GetWeakPtr()));
}

void ClientProxy::OnLocalSocketReadReady() {
  LOG(INFO) << "Initial socket connection comes";
  arc_bridge_socket_controller_.reset();
  vsock_proxy_->Connect(
      base::FilePath(kHostSocketPath),
      base::BindOnce(&ClientProxy::OnConnected, weak_factory_.GetWeakPtr()));
}

void ClientProxy::OnConnected(int error_code, int64_t handle) {
  LOG(INFO) << "Connection in host is done: " << error_code;
  if (error_code == 0) {
    vsock_proxy_->RegisterFileDescriptor(AcceptSocket(arc_bridge_socket_.get()),
                                         arc_proxy::FileDescriptor::SOCKET,
                                         handle);
    LOG(INFO) << "ClientProxy has started to work.";
  }
  arc_bridge_socket_.reset();
}

}  // namespace arc
