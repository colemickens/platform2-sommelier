// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
#include "ippusb_manager/socket_connection.h"

#include <errno.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <utility>

#include <base/files/scoped_file.h>
#include <base/logging.h>

namespace ippusb_manager {

SocketConnection::SocketConnection(base::ScopedFD fd)
    : socket_fd_(std::move(fd)) {}

bool SocketConnection::OpenSocket() {
  // Set options for the socket.
  int val = 1;
  if (setsockopt(socket_fd_.get(), SOL_SOCKET, SO_REUSEADDR, &val,
                 sizeof(val)) < 0) {
    PLOG(ERROR) << "Failed to set socket options";
    return false;
  }

  // Get the bound address of the opened socket.
  socklen_t addrlen = sizeof(addr_);
  if (getsockname(socket_fd_.get(), reinterpret_cast<struct sockaddr*>(&addr_),
                  &addrlen) < 0) {
    PLOG(ERROR) << "Failed to get socket name";
    return false;
  }

  // Verify that the bound address is what we expect.
  if (strcmp(addr_.sun_path, "/run/ippusb/ippusb_manager.sock")) {
    LOG(ERROR) << "Bound socket " << addr_.sun_path
               << " does not match expected address";
    return false;
  }

  // Attempt to listen on the socket for connections.
  if (listen(socket_fd_.get(), 0)) {
    PLOG(ERROR) << "Failed to listen on socket";
    return false;
  }

  return true;
}

// Note: In this function we do not want to call unlink() on the socket. This is
// because the socket was created by upstart and we want it to persist.
void SocketConnection::CloseSocket() {
  socket_fd_.reset();
}

bool SocketConnection::OpenConnection() {
  struct pollfd poll_fd;
  poll_fd.fd = socket_fd_.get();
  poll_fd.events = POLLIN;

  int retval = poll(&poll_fd, 1, 0);
  if (retval < 1) {
    PLOG(INFO) << "The connection isn't ready to be opened yet";
    return false;
  }

  LOG(INFO) << "Socket is ready - attempting to connect";

  int connection_fd = accept(socket_fd_.get(), nullptr, nullptr);
  if (connection_fd < 0) {
    PLOG(ERROR) << "Failed to open connection";
    return false;
  }
  connection_fd_ = base::ScopedFD(connection_fd);

  LOG(INFO) << "Connected to socket";
  return true;
}

void SocketConnection::CloseConnection() {
  shutdown(connection_fd_.get(), SHUT_RDWR);
  connection_fd_.reset();
}

bool SocketConnection::GetMessage(std::string* msg) {
  uint8_t message_length;
  // Receive the length of the message which is stored in the first byte.
  if (recv(connection_fd_.get(), &message_length, 1, MSG_DONTWAIT) < 0) {
    PLOG(ERROR) << "Failed to get message length";
    return false;
  }

  auto buf = std::make_unique<char[]>(message_length);
  ssize_t gotten_size;
  size_t total_size = 0;

  while (total_size < message_length) {
    gotten_size = recv(connection_fd_.get(), buf.get() + total_size,
                       message_length - total_size, MSG_DONTWAIT);
    if (gotten_size < 0) {
      PLOG(ERROR) << "Failed to receive message";
      return false;
    }
    total_size += gotten_size;
  }

  if (total_size > 0) {
    msg->assign(buf.get(), message_length - 1);
    return true;
  }
  return false;
}

bool SocketConnection::SendMessage(const std::string& msg) {
  size_t remaining = msg.size() + 1;
  size_t total = 0;

  // Send the length of the message in the first byte.
  uint8_t message_length = static_cast<uint8_t>(remaining);
  if (send(connection_fd_.get(), &message_length, 1, MSG_NOSIGNAL) < 0) {
    PLOG(ERROR) << "Failed to send message length";
    return false;
  }

  while (remaining > 0) {
    ssize_t sent =
        send(connection_fd_.get(), msg.data() + total, remaining, MSG_NOSIGNAL);

    if (sent < 0) {
      if (errno == EPIPE) {
        connection_is_closed_ = true;
        PLOG(INFO) << "Client closed socket";
        return false;
      }
      PLOG(ERROR) << "Failed to send data over UDS";
      return false;
    }

    total += sent;
    if (sent >= remaining)
      remaining = 0;
    else
      remaining -= sent;
  }

  LOG(INFO) << "Sent " << total << " bytes";
  return true;
}

}  // namespace ippusb_manager
