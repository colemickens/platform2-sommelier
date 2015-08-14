// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <arpa/inet.h>
#include <map>
#include <netdb.h>
#include <string>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <base/bind.h>
#include <base/files/file_util.h>
#include <chromeos/streams/file_stream.h>
#include <chromeos/streams/tls_stream.h>

#include "buffet/socket_stream.h"
#include "buffet/weave_error_conversion.h"

namespace buffet {

namespace {

int ConnectSocket(const std::string& host, uint16_t port) {
  std::string service = std::to_string(port);
  addrinfo hints = {0, AF_UNSPEC, SOCK_STREAM};
  addrinfo* result = nullptr;
  if (getaddrinfo(host.c_str(), service.c_str(), &hints, &result)) {
    PLOG(WARNING) << "Failed to resolve host name: " << host;
    return -1;
  }

  int socket_fd = -1;
  for (const addrinfo* info = result; info != nullptr; info = info->ai_next) {
    socket_fd = socket(info->ai_family, info->ai_socktype, info->ai_protocol);
    if (socket_fd < 0)
      continue;

    char str[INET6_ADDRSTRLEN] = {};
    inet_ntop(info->ai_family, info->ai_addr, str, info->ai_addrlen);
    LOG(INFO) << "Connecting to address: " << str;
    if (connect(socket_fd, info->ai_addr, info->ai_addrlen) == 0)
      break;  // Success.

    PLOG(WARNING) << "Failed to connect to address: " << str;
    close(socket_fd);
    socket_fd = -1;
  }

  freeaddrinfo(result);
  return socket_fd;
}

void OnSuccess(const base::Callback<void(std::unique_ptr<weave::Stream>)>&
                   success_callback,
               chromeos::StreamPtr tls_stream) {
  success_callback.Run(
      std::unique_ptr<weave::Stream>{new SocketStream{std::move(tls_stream)}});
}

void OnError(const base::Callback<void(const weave::Error*)>& error_callback,
             const chromeos::Error* chromeos_error) {
  weave::ErrorPtr error;
  ConvertError(*chromeos_error, &error);
  error_callback.Run(error.get());
}

}  // namespace

bool SocketStream::ReadAsync(
    void* buffer,
    size_t size_to_read,
    const base::Callback<void(size_t)>& success_callback,
    const base::Callback<void(const weave::Error*)>& error_callback,
    weave::ErrorPtr* error) {
  chromeos::ErrorPtr chromeos_error;
  if (!ptr_->ReadAsync(buffer, size_to_read, success_callback,
                       base::Bind(&OnError, error_callback), &chromeos_error)) {
    ConvertError(*chromeos_error, error);
    return false;
  }
  return true;
}

bool SocketStream::WriteAllAsync(
    const void* buffer,
    size_t size_to_write,
    const base::Closure& success_callback,
    const base::Callback<void(const weave::Error*)>& error_callback,
    weave::ErrorPtr* error) {
  chromeos::ErrorPtr chromeos_error;
  if (!ptr_->WriteAllAsync(buffer, size_to_write, success_callback,
                           base::Bind(&OnError, error_callback),
                           &chromeos_error)) {
    ConvertError(*chromeos_error, error);
    return false;
  }
  return true;
}

bool SocketStream::FlushBlocking(weave::ErrorPtr* error) {
  chromeos::ErrorPtr chromeos_error;
  if (!ptr_->FlushBlocking(&chromeos_error)) {
    ConvertError(*chromeos_error, error);
    return false;
  }
  return true;
}

bool SocketStream::CloseBlocking(weave::ErrorPtr* error) {
  chromeos::ErrorPtr chromeos_error;
  if (!ptr_->CloseBlocking(&chromeos_error)) {
    ConvertError(*chromeos_error, error);
    return false;
  }
  return true;
}

void SocketStream::CancelPendingAsyncOperations() {
  ptr_->CancelPendingAsyncOperations();
}

std::unique_ptr<weave::Stream> SocketStream::ConnectBlocking(
    const std::string& host,
    uint16_t port) {
  int socket_fd = ConnectSocket(host, port);
  if (socket_fd <= 0)
    return nullptr;

  auto ptr_ =
      chromeos::FileStream::FromFileDescriptor(socket_fd, true, nullptr);
  if (ptr_)
    return std::unique_ptr<Stream>{new SocketStream{std::move(ptr_)}};

  close(socket_fd);
  return nullptr;
}

void SocketStream::TlsConnect(
    std::unique_ptr<Stream> socket,
    const std::string& host,
    const base::Callback<void(std::unique_ptr<Stream>)>& success_callback,
    const base::Callback<void(const weave::Error*)>& error_callback) {
  SocketStream* stream = static_cast<SocketStream*>(socket.get());
  chromeos::TlsStream::Connect(std::move(stream->ptr_), host,
                               base::Bind(&OnSuccess, success_callback),
                               base::Bind(&OnError, error_callback));
}

}  // namespace buffet
