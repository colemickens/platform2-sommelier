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
#include "arc/vm/vsock_proxy/message.pb.h"
#include "arc/vm/vsock_proxy/proxy_file_system.h"
#include "arc/vm/vsock_proxy/vsock_proxy.h"

namespace arc {

SocketStream::SocketStream(base::ScopedFD socket_fd, VSockProxy* proxy)
    : socket_fd_(std::move(socket_fd)), proxy_(proxy) {
  // TODO(hidehiko): Re-enable DCHECK on production when file descriptor
  // registration part is extracted. Currently nullptr for proxy is allowed
  // for unit testing purpose.
  // DCHECK(proxy);
}

SocketStream::~SocketStream() = default;

bool SocketStream::Read(arc_proxy::VSockMessage* message) {
  char buf[4096];
  std::vector<base::ScopedFD> fds;
  ssize_t size =
      base::UnixDomainSocket::RecvMsg(socket_fd_.get(), buf, sizeof(buf), &fds);
  if (size == -1) {
    PLOG(ERROR) << "Failed to recieve a message";
    return false;
  }

  if (size == 0 && fds.empty()) {
    LOG(INFO) << "EOF is found.";
    message->mutable_close();
    return true;
  }

  // Validate file descriptor type before registering to VSockProxy.
  struct FileDescriptorAttr {
    arc_proxy::FileDescriptor::Type type;
    uint64_t size;
  };
  std::vector<FileDescriptorAttr> fd_attrs;
  fd_attrs.reserve(fds.size());
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
          fd_attrs.emplace_back(
              FileDescriptorAttr{arc_proxy::FileDescriptor::FIFO_READ, 0});
          break;
        case O_WRONLY:
          fd_attrs.emplace_back(
              FileDescriptorAttr{arc_proxy::FileDescriptor::FIFO_WRITE, 0});
          break;
        default:
          LOG(ERROR) << "Unsupported access mode: " << (flags & O_ACCMODE);
          return false;
      }
      continue;
    }
    if (S_ISSOCK(st.st_mode)) {
      fd_attrs.emplace_back(
          FileDescriptorAttr{arc_proxy::FileDescriptor::SOCKET, 0});
      continue;
    }
    if (S_ISREG(st.st_mode)) {
      fd_attrs.emplace_back(
          FileDescriptorAttr{arc_proxy::FileDescriptor::REGULAR_FILE,
                             static_cast<uint64_t>(st.st_size)});
      continue;
    }

    LOG(ERROR) << "Unsupported FD type: " << st.st_mode;
    return false;
  }
  DCHECK_EQ(fds.size(), fd_attrs.size());

  // Build returning message.
  auto* data = message->mutable_data();
  data->set_blob(buf, size);
  for (size_t i = 0; i < fds.size(); ++i) {
    int64_t handle = proxy_->RegisterFileDescriptor(
        std::move(fds[i]), fd_attrs[i].type, 0 /* generate handle */);
    auto* transferred_fd = data->add_transferred_fd();
    transferred_fd->set_handle(handle);
    transferred_fd->set_type(fd_attrs[i].type);
    if (fd_attrs[i].type == arc_proxy::FileDescriptor::REGULAR_FILE)
      transferred_fd->set_file_size(fd_attrs[i].size);
  }
  return true;
}

bool SocketStream::Write(arc_proxy::Data* data) {
  // First, create file descriptors for the received message.
  std::vector<base::ScopedFD> transferred_fds;
  transferred_fds.reserve(data->transferred_fd().size());
  for (const auto& transferred_fd : data->transferred_fd()) {
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
      case arc_proxy::FileDescriptor::REGULAR_FILE: {
        auto* proxy_file_system = proxy_->proxy_file_system();
        if (!proxy_file_system)
          return false;
        // Create a file descriptor which is handled by |proxy_file_system|.
        remote_fd = proxy_file_system->RegisterHandle(transferred_fd.handle());
        if (!remote_fd.is_valid())
          return false;
        break;
      }
      default:
        LOG(ERROR) << "Unsupported FD type: " << transferred_fd.type();
        return false;
    }

    // |local_fd| is set iff the descriptor's read readiness needs to be
    // watched, so register it.
    if (local_fd.is_valid()) {
      proxy_->RegisterFileDescriptor(std::move(local_fd), transferred_fd.type(),
                                     transferred_fd.handle());
    }
    transferred_fds.emplace_back(std::move(remote_fd));
  }

  std::vector<int> fds;
  fds.reserve(transferred_fds.size());
  for (const auto& fd : transferred_fds)
    fds.push_back(fd.get());

  const auto& blob = data->blob();
  if (!base::UnixDomainSocket::SendMsg(socket_fd_.get(), blob.data(),
                                       blob.size(), fds)) {
    PLOG(ERROR) << "Failed to send message";
    return false;
  }

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

}  // namespace arc
