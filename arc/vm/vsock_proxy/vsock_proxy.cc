// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "arc/vm/vsock_proxy/vsock_proxy.h"

#include <errno.h>

#include <utility>

#include <base/bind.h>
#include <base/files/file_path.h>
#include <base/logging.h>

#include "arc/vm/vsock_proxy/file_descriptor_util.h"
#include "arc/vm/vsock_proxy/file_stream.h"
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
    case arc_proxy::FileDescriptor::REGULAR_FILE:
      return std::make_unique<FileStream>(std::move(fd));
    default:
      LOG(ERROR) << "Unknown FileDescriptor::Type: " << fd_type;
      return nullptr;
  }
}

}  // namespace

VSockProxy::VSockProxy(Type type,
                       ProxyFileSystem* proxy_file_system,
                       base::ScopedFD vsock)
    : type_(type),
      proxy_file_system_(proxy_file_system),
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
  std::unique_ptr<base::FileDescriptorWatcher::Controller> controller;
  if (fd_type != arc_proxy::FileDescriptor::REGULAR_FILE) {
    controller = base::FileDescriptorWatcher::WatchReadable(
        raw_fd, base::BindRepeating(&VSockProxy::OnLocalFileDesciptorReadReady,
                                    weak_factory_.GetWeakPtr(), handle));
  }
  fd_map_.emplace(handle,
                  FileDescriptorInfo{std::move(stream), std::move(controller)});

  // TODO(hidehiko): Info looks too verbose. Reduce it when we are ready.
  LOG(INFO) << "New FD is created: raw_fd=" << raw_fd << ", handle=" << handle;
  return handle;
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
    fd_map_.clear();
    vsock_controller_.reset();
    std::move(callback).Run(ECONNREFUSED, 0 /* invalid */);
    return;
  }
  pending_connect_.emplace(cookie, std::move(callback));
}

void VSockProxy::Pread(int64_t handle,
                       uint64_t count,
                       uint64_t offset,
                       PreadCallback callback) {
  // TODO(hidehiko): Ensure cookie is unique in case of overflow.
  const int64_t cookie = next_cookie_;
  if (type_ == Type::SERVER)
    ++next_cookie_;
  else
    --next_cookie_;

  arc_proxy::VSockMessage message;
  auto* request = message.mutable_pread_request();
  request->set_cookie(cookie);
  request->set_handle(handle);
  request->set_count(count);
  request->set_offset(offset);
  if (!vsock_.Write(message)) {
    // Failed to write a message to VSock. Delete everything.
    fd_map_.clear();
    vsock_controller_.reset();
    std::move(callback).Run(ECONNREFUSED, 0 /* invalid */);
    return;
  }
  pending_pread_.emplace(cookie, std::move(callback));
}

void VSockProxy::Close(int64_t handle) {
  arc_proxy::VSockMessage message;
  message.mutable_close()->set_handle(handle);
  if (!vsock_.Write(message)) {
    // Failed to write a message to VSock. Delete everything.
    fd_map_.clear();
    vsock_controller_.reset();
  }
}

void VSockProxy::OnVSockReadReady() {
  arc_proxy::VSockMessage message;
  if (!vsock_.Read(&message)) {
    // TODO(hidehiko): Support VSOCK close case.
    // Failed to read a message from VSock. Delete everything.
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
    case arc_proxy::VSockMessage::kPreadRequest: {
      OnPreadRequest(message.mutable_pread_request());
      return;
    }
    case arc_proxy::VSockMessage::kPreadResponse: {
      OnPreadResponse(message.mutable_pread_response());
      return;
    }
    default:
      LOG(ERROR) << "Unknown message type: " << message.command_case();
  }
}

void VSockProxy::OnClose(arc_proxy::Close* close) {
  LOG(INFO) << "Closing: " << close->handle();
  auto it = fd_map_.find(close->handle());
  if (it == fd_map_.end()) {
    LOG(ERROR) << "Couldn't find handle: handle=" << close->handle();
    return;
  }
  fd_map_.erase(it);
}

void VSockProxy::OnData(arc_proxy::Data* data) {
  auto it = fd_map_.find(data->handle());
  if (it == fd_map_.end()) {
    LOG(ERROR) << "Couldn't find handle: handle=" << data->handle();
    return;
  }

  // TODO(b/123613033): Fix the error handling. Specifically, if the socket
  // buffer is full, EAGAIN will be returned. The case needs to be rescued
  // at least.
  if (!it->second.stream->Write(data)) {
    LOG(ERROR) << "Failed to write to a file descriptor: handle="
               << data->handle();
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

void VSockProxy::OnPreadRequest(arc_proxy::PreadRequest* request) {
  arc_proxy::VSockMessage reply;
  auto* response = reply.mutable_pread_response();
  response->set_cookie(request->cookie());

  OnPreadRequestInternal(request, response);

  if (!vsock_.Write(reply)) {
    // Failed to write a message to VSock. Delete everything.
    fd_map_.clear();
    vsock_controller_.reset();
  }
}

void VSockProxy::OnPreadRequestInternal(arc_proxy::PreadRequest* request,
                                        arc_proxy::PreadResponse* response) {
  auto it = fd_map_.find(request->handle());
  if (it == fd_map_.end()) {
    LOG(ERROR) << "Couldn't find handle: handle=" << request->handle();
    response->set_error_code(EBADF);
    return;
  }

  if (!it->second.stream->Pread(request->count(), request->offset(),
                                response)) {
    response->set_error_code(EINVAL);
    return;
  }
}

void VSockProxy::OnPreadResponse(arc_proxy::PreadResponse* response) {
  auto it = pending_pread_.find(response->cookie());
  if (it == pending_pread_.end()) {
    LOG(ERROR) << "Unexpected pread response: cookie=" << response->cookie();
    return;
  }

  auto callback = std::move(it->second);
  pending_pread_.erase(it);
  std::move(callback).Run(response->error_code(),
                          std::move(*response->mutable_blob()));
}

void VSockProxy::OnLocalFileDesciptorReadReady(int64_t handle) {
  auto it = fd_map_.find(handle);
  if (it == fd_map_.end()) {
    LOG(ERROR) << "Unknown FD gets read ready: handle=" << handle;
    return;
  }

  arc_proxy::VSockMessage message;
  if (!it->second.stream->Read(&message)) {
    LOG(ERROR) << "Failed to read from file descriptor. handle=" << handle;
    // Notify to the other side to close.
    message.Clear();
    message.mutable_close();
  }

  if (message.has_close()) {
    // In case of EOF on the other side of the |fd|, |fd| needs to be closed.
    // Otherwise it will be kept read-ready and this callback will be
    // repeatedly called.
    LOG(INFO) << "Closing: handle=" << handle;
    message.mutable_close()->set_handle(handle);
    // Close the corresponding fd, too.
    fd_map_.erase(it);
  } else {
    DCHECK(message.has_data());
    message.mutable_data()->set_handle(handle);
  }
  if (!vsock_.Write(message)) {
    // Failed to write a message to VSock. Delete everything.
    fd_map_.clear();
    vsock_controller_.reset();
  }
}

}  // namespace arc
