// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "arc/vm/vsock_proxy/socket_stream.h"

#include <errno.h>

#include <string>
#include <utility>
#include <vector>

#include <base/bind.h>
#include <base/logging.h>
#include <base/posix/unix_domain_socket.h>

namespace arc {

SocketStream::SocketStream(base::ScopedFD socket_fd)
    : socket_fd_(std::move(socket_fd)) {}

SocketStream::~SocketStream() = default;

StreamBase::ReadResult SocketStream::Read() {
  std::string buf;
  buf.resize(4096);
  std::vector<base::ScopedFD> fds;
  ssize_t size = base::UnixDomainSocket::RecvMsg(socket_fd_.get(), &buf[0],
                                                 buf.size(), &fds);
  if (size == -1) {
    int error_code = errno;
    PLOG(ERROR) << "Failed to recieve a message";
    return {error_code, std::string(), {}};
  }

  buf.resize(size);
  return {0 /* succeed */, std::move(buf), std::move(fds)};
}

bool SocketStream::Write(std::string blob, std::vector<base::ScopedFD> fds) {
  pending_write_.emplace_back(Data{std::move(blob), std::move(fds)});
  if (!writable_watcher_)  // TrySendMsg will be called later if watching.
    TrySendMsg();
  return true;
}

bool SocketStream::Pread(uint64_t count,
                         uint64_t offset,
                         arc_proxy::PreadResponse* response) {
  LOG(ERROR) << "Pread for socket file descriptor is unsupported.";
  return false;
}

bool SocketStream::Fstat(arc_proxy::FstatResponse* response) {
  LOG(ERROR) << "Fstat for socket file descriptor is unsupported.";
  return false;
}

void SocketStream::TrySendMsg() {
  DCHECK(!pending_write_.empty());
  for (; !pending_write_.empty(); pending_write_.pop_front()) {
    const auto& data = pending_write_.front();

    std::vector<int> raw_fds;
    raw_fds.reserve(data.fds.size());
    for (const auto& fd : data.fds)
      raw_fds.push_back(fd.get());

    if (!base::UnixDomainSocket::SendMsg(socket_fd_.get(), data.blob.data(),
                                         data.blob.size(), raw_fds)) {
      if (errno == EAGAIN) {
        // Will retry later.
        if (!writable_watcher_) {
          writable_watcher_ = base::FileDescriptorWatcher::WatchWritable(
              socket_fd_.get(),
              base::BindRepeating(&SocketStream::TrySendMsg,
                                  weak_factory_.GetWeakPtr()));
        }
        return;
      }
      PLOG(ERROR) << "Failed to send message";
    }
  }
  // No pending data left. Stop watching.
  writable_watcher_.reset();
}

}  // namespace arc
