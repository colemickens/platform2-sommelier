// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPPUSB_MANAGER_SOCKET_CONNECTION_H_
#define IPPUSB_MANAGER_SOCKET_CONNECTION_H_

#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>

#include <string>

#include <base/files/scoped_file.h>
#include <base/macros.h>

namespace ippusb_manager {

class SocketConnection {
 public:
  explicit SocketConnection(base::ScopedFD fd);

  // Attempts to listen open the socket defined by |socket_fd_|. Returns true if
  // the socket was successfully opened, false otherwise.
  bool OpenSocket();

  void CloseSocket();

  // Attempts to accept a client connection on the open socket. Returns true if
  // the connection is opened successfully, false otherwise.
  bool OpenConnection();

  // Closes the client connection.
  void CloseConnection();

  // Attempts to receive a message from the client connection and returns it as
  // a string.
  bool GetMessage(std::string* msg);

  // Attempts to send |msg| to the client through the open connection.
  bool SendMessage(const std::string& msg);

 private:
  // File descriptor of the socket.
  base::ScopedFD socket_fd_;
  struct sockaddr_un addr_;

  // File descriptor for the currently open connection.
  base::ScopedFD connection_fd_;

  // Represents whether or not the connection is closed.
  bool connection_is_closed_;

  DISALLOW_COPY_AND_ASSIGN(SocketConnection);
};

}  // namespace ippusb_manager

#endif  // IPPUSB_MANAGER_SOCKET_CONNECTION_H_
