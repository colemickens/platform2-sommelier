// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "hermes/socket_qrtr.h"

#include <libqrtr.h>

namespace {

constexpr uint8_t kQrtrPort = 0;

}  // namespace

namespace hermes {

SocketQrtr::SocketQrtr()
    : node_(0),
      port_(0),
      watcher_(FROM_HERE) {}

void SocketQrtr::SetDataAvailableCallback(DataAvailableCallback cb) {
  cb_ = cb;
}

bool SocketQrtr::Open() {
  if (IsValid()) {
    return true;
  }

  socket_.reset(qrtr_open(kQrtrPort));
  if (!socket_.is_valid()) {
    LOG(ERROR) << "Failed to open QRTR socket with port " << kQrtrPort;
    return false;
  }

  if (!base::MessageLoopForIO::current()->WatchFileDescriptor(
          socket_.get(), true /* persistent */,
          base::MessageLoopForIO::WATCH_READ, &watcher_, this)) {
    LOG(ERROR) << "Failed to set up WatchFileDescriptor";
    socket_.reset();
    return false;
  }
  return true;
}

void SocketQrtr::Close() {
  if (IsValid()) {
    qrtr_close(socket_.get());
    socket_.reset();
  }
}

bool SocketQrtr::StartService(uint32_t service, uint16_t version_major,
                              uint16_t version_minor) {
  return qrtr_new_lookup(socket_.get(), service, version_major,
                         version_minor) >= 0;
}

bool SocketQrtr::StopService(uint32_t service, uint16_t version_major,
                             uint16_t version_minor) {
  return qrtr_remove_lookup(socket_.get(), service, version_major,
                            version_minor) >= 0;
}

int SocketQrtr::Recv(void* buf, size_t size, void* metadata) {
  int ret = qrtr_recvfrom(socket_.get(), buf, size, &node_, &port_);
  if (metadata) {
    PacketMetadata* data = reinterpret_cast<PacketMetadata*>(metadata);
    data->node = node_;
    data->port = port_;
  }
  return ret;
}

int SocketQrtr::Send(const void* data, size_t size) {
  return qrtr_sendto(socket_.get(), node_, port_, data, size);
}

void SocketQrtr::OnFileCanReadWithoutBlocking(int socket) {
  if (cb_) {
    cb_.Run(this);
  }
}

void SocketQrtr::OnFileCanWriteWithoutBlocking(int socket) {
  NOTREACHED() << "Not watching file descriptor for write";
}

}  // namespace hermes
