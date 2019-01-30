// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "arc/vm/vsock_proxy/vsock_proxy.h"

#include <utility>

#include <base/bind.h>
#include <base/files/file_path.h>
#include <base/logging.h>

#include "arc/vm/vsock_proxy/file_descriptor_util.h"
#include "arc/vm/vsock_proxy/pipe_stream.h"
#include "arc/vm/vsock_proxy/socket_stream.h"

namespace arc {
namespace {

std::unique_ptr<StreamBase> CreateStream(
    base::ScopedFD fd,
    arc_proxy::FileDescriptor::Type fd_type,
    VSockProxy* proxy) {
  switch (fd_type) {
    case arc_proxy::FileDescriptor::SOCKET:
      return std::make_unique<SocketStream>(std::move(fd), proxy);
    case arc_proxy::FileDescriptor::FIFO_READ:
    case arc_proxy::FileDescriptor::FIFO_WRITE:
      return std::make_unique<PipeStream>(std::move(fd));
    default:
      LOG(ERROR) << "Unknown FileDescriptor::Type: " << fd_type;
      return nullptr;
  }
}

}  // namespace

VSockProxy::VSockProxy(Type type, base::ScopedFD vsock)
    : type_(type),
      vsock_(std::move(vsock)),
      next_handle_(type == Type::SERVER ? 1 : -1),
      next_cookie_(type == Type::SERVER ? 1 : -1) {
  // Note: this needs to be initialized after mWeakFxactory, which is
  // declared after mVSockController in order to destroy it at first.
  vsock_controller_ = base::FileDescriptorWatcher::WatchReadable(
      vsock_.Get(), base::BindRepeating(&VSockProxy::OnVSockReadReady,
                                        weak_factory_.GetWeakPtr()));
}

VSockProxy::~VSockProxy() = default;

int64_t VSockProxy::RegisterFileDescriptor(
    base::ScopedFD fd,
    arc_proxy::FileDescriptor::Type fd_type,
    int64_t handle) {
  if (!fd.is_valid()) {
    LOG(ERROR) << "Registering invalid fd.";
    return 0;
  }

  const int raw_fd = fd.get();
  if (handle == 0) {
    // TODO(hidehiko): Ensure handle is unique in case of overflow.
    if (type_ == Type::SERVER)
      handle = next_handle_++;
    else
      handle = next_handle_--;
  }

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

void VSockProxy::Connect(const base::FilePath& path, ConnectCallback callback) {
  // TODO(hidehiko): Ensure cookie is unique in case of overflow.
  const int64_t cookie = next_cookie_;
  if (type_ == Type::SERVER)
    ++next_cookie_;
  else
    --next_cookie_;

  arc_proxy::VSockMessage message;
  auto* request = message.mutable_connect_request();
  request->set_cookie(cookie);
  request->set_path(path.value());
  if (!vsock_.Write(message)) {
    // Failed to write a message to VSock. Delete everything.
    handle_map_.clear();
    fd_map_.clear();
    vsock_controller_.reset();
    std::move(callback).Run(ECONNREFUSED, 0 /* invalid */);
    return;
  }
  pending_connect_.emplace(cookie, std::move(callback));
}

void VSockProxy::OnVSockReadReady() {
  arc_proxy::VSockMessage message;
  if (!vsock_.Read(&message)) {
    // TODO(hidehiko): Support VSOCK close case.
    // Failed to read a message from VSock. Delete everything.
    handle_map_.clear();
    fd_map_.clear();
    vsock_controller_.reset();
    return;
  }

  switch (message.command_case()) {
    case arc_proxy::VSockMessage::kClose:
      OnClose(message.mutable_close());
      return;
    case arc_proxy::VSockMessage::kData:
      OnData(message.mutable_data());
      return;
    case arc_proxy::VSockMessage::kConnectRequest: {
      OnConnectRequest(message.mutable_connect_request());
      return;
    }
    case arc_proxy::VSockMessage::kConnectResponse:
      OnConnectResponse(message.mutable_connect_response());
      return;
    default:
      LOG(ERROR) << "Unknown message type: " << message.command_case();
  }
}

void VSockProxy::OnClose(arc_proxy::Close* close) {
  LOG(INFO) << "Closing: " << close->handle();
  auto it = handle_map_.find(close->handle());
  if (it == handle_map_.end()) {
    LOG(ERROR) << "Couldn't find handle: handle=" << close->handle();
    return;
  }

  int raw_fd = it->second;
  handle_map_.erase(it);
  if (fd_map_.erase(raw_fd) == 0) {
    LOG(ERROR) << "Couldn't find fd in fd_map: handle=" << close->handle()
               << ", fd=" << raw_fd;
  }
}

void VSockProxy::OnData(arc_proxy::Data* data) {
  auto it = handle_map_.find(data->handle());
  if (it == handle_map_.end()) {
    LOG(ERROR) << "Couldn't find handle: handle=" << data->handle();
    return;
  }

  auto fd_it = fd_map_.find(it->second);
  if (fd_it == fd_map_.end()) {
    LOG(ERROR) << "Couldn't find fd: handle=" << data->handle()
               << ", fd=" << it->second;
    return;
  }

  // TODO(b/123613033): Fix the error handling. Specifically, if the socket
  // buffer is full, EAGAIN will be returned. The case needs to be rescued
  // at least.
  if (!fd_it->second.stream->Write(data)) {
    LOG(ERROR) << "Failed to write to a file descriptor: handle="
               << data->handle() << ", fd=" << it->second;
  }
}

void VSockProxy::OnConnectRequest(arc_proxy::ConnectRequest* request) {
  LOG(INFO) << "Connecting to " << request->path();
  arc_proxy::VSockMessage reply;
  auto* response = reply.mutable_connect_response();

  // Currently, this actually uses only on ArcBridgeService's initial
  // connection establishment, and the request comes from the guest to the host
  // including the |path|.
  // TODO(hidehiko): Consider whitelist the path which is allowed to access.
  auto result = ConnectUnixDomainSocket(base::FilePath(request->path()));
  response->set_cookie(request->cookie());
  response->set_error_code(result.first);
  if (result.first == 0) {
    response->set_handle(RegisterFileDescriptor(
        std::move(result.second), arc_proxy::FileDescriptor::SOCKET,
        0 /* generate handle */));
  }

  if (!vsock_.Write(reply)) {
    // Failed to write a message to VSock. Delete everything.
    handle_map_.clear();
    fd_map_.clear();
    vsock_controller_.reset();
  }
}

void VSockProxy::OnConnectResponse(arc_proxy::ConnectResponse* response) {
  auto it = pending_connect_.find(response->cookie());
  if (it == pending_connect_.end()) {
    LOG(ERROR) << "Unexpected connect response: cookie=" << response->cookie();
    return;
  }

  auto callback = std::move(it->second);
  pending_connect_.erase(it);
  std::move(callback).Run(response->error_code(), response->handle());
}

void VSockProxy::OnLocalFileDesciptorReadReady(int fd) {
  auto fd_it = fd_map_.find(fd);
  if (fd_it == fd_map_.end()) {
    LOG(ERROR) << "Unknown FD gets read ready: fd=" << fd;
    return;
  }

  arc_proxy::VSockMessage message;
  if (!fd_it->second.stream->Read(&message)) {
    LOG(ERROR) << "Failed to read from file descriptor. "
               << "handle=" << fd_it->second.handle << ", fd=" << fd_it->first;
    // Notify to the other side to close.
    message.Clear();
    message.mutable_close();
  }

  if (message.has_close()) {
    // In case of EOF on the other side of the |fd|, |fd| needs to be closed.
    // Otherwise it will be kept read-ready and this callback will be
    // repeatedly called.
    LOG(INFO) << "Closing: handle=" << fd_it->second.handle
              << ", fd=" << fd_it->first;
    message.mutable_close()->set_handle(fd_it->second.handle);
    // Close the corresponding fd, too.
    handle_map_.erase(fd_it->second.handle);
    fd_map_.erase(fd_it);
  } else {
    DCHECK(message.has_data());
    message.mutable_data()->set_handle(fd_it->second.handle);
  }
  if (!vsock_.Write(message)) {
    // Failed to write a message to VSock. Delete everything.
    handle_map_.clear();
    fd_map_.clear();
    vsock_controller_.reset();
  }
}

}  // namespace arc
