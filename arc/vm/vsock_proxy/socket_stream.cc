// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "arc/vm/vsock_proxy/socket_stream.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <tuple>
#include <utility>
#include <vector>

#include <base/logging.h>
#include <base/posix/unix_domain_socket_linux.h>

#include "arc/vm/vsock_proxy/file_descriptor_util.h"
#include "arc/vm/vsock_proxy/vsock_proxy.h"

namespace arc {

SocketStream::SocketStream(base::ScopedFD socket_fd, VSockProxy* proxy)
    : socket_fd_(std::move(socket_fd)), proxy_(proxy) {
  // TODO(hidehiko): Re-enable DCHECK on production.
  // Currently nullptr for proxy is allowed for unit testing purpose.
  // DCHECK(proxy);
}

SocketStream::~SocketStream() = default;

bool SocketStream::Read(arc_proxy::Message* message) {
  char buf[4096];
  std::vector<base::ScopedFD> fds;
  ssize_t size =
      base::UnixDomainSocket::RecvMsg(socket_fd_.get(), buf, sizeof(buf), &fds);
  if (size == -1) {
    PLOG(ERROR) << "Failed to recieve a message";
    return false;
  }

  if (size == 0 && fds.empty()) {
    LOG(INFO) << "EOF Found";
    // Set 0 length data to indicate the other side of the FD is closed.
    message->Clear();
    return true;
  }

  // Validate file descriptor type before registering to VSockProxy.
  std::vector<arc_proxy::FileDescriptor::Type> fd_types;
  fd_types.reserve(fds.size());
  for (const auto& fd : fds) {
    struct stat st;
    if (fstat(fd.get(), &st) == -1) {
      PLOG(ERROR) << "Failed to fstat";
      return false;
    }

    if (S_ISFIFO(st.st_mode)) {
      int flags = fcntl(fd.get(), F_GETFL, 0);
      if (flags < 0) {
        PLOG(ERROR) << "Failed to find file status flags";
        return false;
      }
      switch (flags & O_ACCMODE) {
        case O_RDONLY:
          fd_types.push_back(arc_proxy::FileDescriptor::FIFO_READ);
          break;
        case O_WRONLY:
          fd_types.push_back(arc_proxy::FileDescriptor::FIFO_WRITE);
          break;
        default:
          PLOG(ERROR) << "Unsupported access mode: " << (flags & O_ACCMODE);
          return false;
      }
      continue;
    }
    if (S_ISSOCK(st.st_mode)) {
      fd_types.push_back(arc_proxy::FileDescriptor::SOCKET);
      continue;
    }

    LOG(ERROR) << "Unsupported FD type: " << st.st_mode;
    return false;
  }
  DCHECK_EQ(fds.size(), fd_types.size());

  // Build returning message.
  message->set_data(buf, size);
  for (size_t i = 0; i < fds.size(); ++i) {
    uint64_t handle = proxy_->RegisterFileDescriptor(
        std::move(fds[i]), fd_types[i], 0 /* generate handle */);
    auto* transferred_fd = message->add_transferred_fd();
    transferred_fd->set_handle(handle);
    transferred_fd->set_type(fd_types[i]);
  }

  return true;
}

bool SocketStream::Write(const arc_proxy::Message& message) {
  // First, create file descriptors for the received message.
  std::vector<base::ScopedFD> transferred_fds;
  transferred_fds.reserve(message.transferred_fd().size());
  for (const auto& transferred_fd : message.transferred_fd()) {
    base::ScopedFD local_fd;
    base::ScopedFD remote_fd;
    switch (transferred_fd.type()) {
      case arc_proxy::FileDescriptor::FIFO_READ: {
        auto created = CreatePipe();
        if (!created)
          return false;
        std::tie(remote_fd, local_fd) = std::move(*created);
        break;
      }
      case arc_proxy::FileDescriptor::FIFO_WRITE: {
        auto created = CreatePipe();
        if (!created)
          return false;
        std::tie(local_fd, remote_fd) = std::move(*created);
        break;
      }
      case arc_proxy::FileDescriptor::SOCKET: {
        auto created = CreateSocketPair();
        if (!created)
          return false;
        std::tie(local_fd, remote_fd) = std::move(*created);
        break;
      }
      default:
        LOG(ERROR) << "Unsupported FD type: " << transferred_fd.type();
        return false;
    }

    proxy_->RegisterFileDescriptor(std::move(local_fd), transferred_fd.type(),
                                   transferred_fd.handle());
    transferred_fds.emplace_back(std::move(remote_fd));
  }

  std::vector<int> fds;
  fds.reserve(transferred_fds.size());
  for (const auto& fd : transferred_fds)
    fds.push_back(fd.get());

  if (!base::UnixDomainSocket::SendMsg(socket_fd_.get(), message.data().data(),
                                       message.data().size(), fds)) {
    PLOG(ERROR) << "Failed to send message";
    return false;
  }

  return true;
}

}  // namespace arc
