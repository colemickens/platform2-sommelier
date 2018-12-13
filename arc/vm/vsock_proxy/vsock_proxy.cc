// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "arc/vm/vsock_proxy/vsock_proxy.h"

#include <utility>

#include <base/bind.h>
#include <base/logging.h>

#include "arc/vm/vsock_proxy/pipe_stream.h"
#include "arc/vm/vsock_proxy/socket_stream.h"

namespace arc {
namespace {

std::unique_ptr<StreamBase> CreateStream(
    base::ScopedFD fd,
    arc_proxy::FileDescriptor::Type fd_type,
    VSockProxy* proxy) {
  switch (fd_type) {
    case arc_proxy::FileDescriptor::FIFO_READ:
    case arc_proxy::FileDescriptor::FIFO_WRITE:
      return std::make_unique<PipeStream>(std::move(fd));
    case arc_proxy::FileDescriptor::SOCKET:
      return std::make_unique<SocketStream>(std::move(fd), proxy);
    default:
      LOG(ERROR) << "Unknown FileDescriptor::Type: " << fd_type;
      return nullptr;
  }
}

}  // namespace

VSockProxy::VSockProxy(base::ScopedFD vsock) : vsock_(std::move(vsock)) {
  // Note: this needs to be initialized after mWeakFactory, which is
  // declared after mVSockController in order to destroy it at first.
  vsock_controller_ = base::FileDescriptorWatcher::WatchReadable(
      vsock_.Get(), base::BindRepeating(&VSockProxy::OnVSockReadReady,
                                        weak_factory_.GetWeakPtr()));
}

VSockProxy::~VSockProxy() = default;

uint64_t VSockProxy::RegisterFileDescriptor(
    base::ScopedFD fd,
    arc_proxy::FileDescriptor::Type fd_type,
    uint64_t handle) {
  if (!fd.is_valid()) {
    LOG(ERROR) << "Registering invalid fd.";
    return 0;
  }

  const int raw_fd = fd.get();
  // TODO(hidehiko,yusukes,keiichiw): Decide the precise protocol.
  if (handle == 0)
    handle = next_handle_++;

  auto stream = CreateStream(std::move(fd), fd_type, this);
  auto controller = base::FileDescriptorWatcher::WatchReadable(
      raw_fd, base::BindRepeating(&VSockProxy::OnLocalFileDesciptorReadReady,
                                  weak_factory_.GetWeakPtr(), raw_fd));
  fd_map_.emplace(raw_fd, FileDescriptorInfo{handle, std::move(stream),
                                             std::move(controller)});
  handle_map_.emplace(handle, raw_fd);

  // TODO(hidehiko): Info looks too verbose. Reduce it when we are ready.
  LOG(INFO) << "New FD is created: raw_fd=" << raw_fd << ", handle=" << handle;
  return handle;
}

void VSockProxy::UnregisterFileDescriptor(int fd) {
  auto iter = fd_map_.find(fd);
  if (iter == fd_map_.end()) {
    LOG(ERROR) << "Unregistering unknown fd: fd=" << fd;
    return;
  }
  handle_map_.erase(iter->second.handle);
  fd_map_.erase(iter);
}

void VSockProxy::OnVSockReadReady() {
  auto message = vsock_.Read();
  if (!message) {
    // TODO(hidehiko): Support VSOCK close case.
    // Failed to read a message from VSock. Delete everything.
    handle_map_.clear();
    fd_map_.clear();
    vsock_controller_.reset();
    return;
  }

  auto it = handle_map_.find(message->handle());
  if (it == handle_map_.end()) {
    LOG(ERROR) << "Couldn't find handle: handle=" << message->handle();
    return;
  }

  auto fd_it = fd_map_.find(it->second);
  if (fd_it == fd_map_.end()) {
    LOG(ERROR) << "Couldn't find fd: fd=" << it->second;
    return;
  }

  if (message->data().size() == 0) {
    // At the moment, empty data means that the corresponding file
    // descriptor is closed.
    // TODO(hidehiko,yusukes,keiichiw): Fix the protocol.
    LOG(INFO) << "Close file descriptor: handle=" << message->handle()
              << ", fd=" << it->second;
    handle_map_.erase(it);
    fd_map_.erase(fd_it);
    return;
  }

  if (!fd_it->second.stream->Write(std::move(message).value())) {
    LOG(ERROR) << "Failed to write to file descriptor. Closing... "
               << "handle=" << message->handle() << ", fd=" << it->second;
    handle_map_.erase(it);
    fd_map_.erase(fd_it);
  }
}

void VSockProxy::OnLocalFileDesciptorReadReady(int fd) {
  auto fd_it = fd_map_.find(fd);
  if (fd_it == fd_map_.end()) {
    LOG(ERROR) << "Unknown FD gets read ready: fd=" << fd;
    return;
  }

  auto message = fd_it->second.stream->Read();
  if (!message) {
    // TODO(hidehiko): Update the log message. It may be intentional close.
    LOG(ERROR) << "Failed to read from file descriptor. Closing... "
               << "handle=" << fd_it->second.handle << ", fd=" << fd_it->first;
    handle_map_.erase(fd_it->second.handle);
    fd_map_.erase(fd_it);

    // Create an empty message, indicating the close.
    // TODO(hidehiko,yusukes,keiichiw): Fix the protocol.
    message = arc_proxy::Message();
  }

  message->set_handle(fd_it->second.handle);
  if (!vsock_.Write(std::move(message).value())) {
    // Failed to write a message to VSock. Delete everything.
    handle_map_.clear();
    fd_map_.clear();
    vsock_controller_.reset();
  }
}

}  // namespace arc
