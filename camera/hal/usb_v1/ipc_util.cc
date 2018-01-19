/*
 * Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "hal/usb_v1/ipc_util.h"

#include <errno.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <string>

#include <base/files/file_util.h>
#include <base/files/scoped_file.h>
#include <base/logging.h>

namespace internal {

static const size_t kMaxSocketNameLength = 104;

// The following four functions were taken from
// ipc/unix_domain_socket_util.{h,cc}.
int MakeUnixAddrForPath(const std::string& socket_name,
                        struct sockaddr_un* unix_addr,
                        size_t* unix_addr_len) {
  DCHECK(unix_addr);
  DCHECK(unix_addr_len);

  if (socket_name.length() == 0) {
    LOG(ERROR) << "Empty socket name provided for unix socket address.";
    return -1;
  }
  // We reject socket_name.length() == kMaxSocketNameLength to make room for
  // the NUL terminator at the end of the string.
  if (socket_name.length() >= kMaxSocketNameLength) {
    LOG(ERROR) << "Socket name too long: " << socket_name;
    return -1;
  }

  // Create socket.
  base::ScopedFD fd(socket(AF_UNIX, SOCK_STREAM, 0));
  if (!fd.is_valid()) {
    PLOG(ERROR) << "socket";
    return -1;
  }

  // Make socket non-blocking
  if (HANDLE_EINTR(fcntl(fd.get(), F_SETFL, O_NONBLOCK)) < 0) {
    PLOG(ERROR) << "fcntl(O_NONBLOCK)";
    return -1;
  }

  // Create unix_addr structure.
  memset(unix_addr, 0, sizeof(struct sockaddr_un));
  unix_addr->sun_family = AF_UNIX;
  strncpy(unix_addr->sun_path, socket_name.c_str(), kMaxSocketNameLength);
  *unix_addr_len =
      offsetof(struct sockaddr_un, sun_path) + socket_name.length();
  return fd.release();
}

bool CreateServerUnixDomainSocket(const base::FilePath& socket_path,
                                  int* server_listen_fd) {
  DCHECK(server_listen_fd);

  std::string socket_name = socket_path.value();
  base::FilePath socket_dir = socket_path.DirName();

  struct sockaddr_un unix_addr;
  size_t unix_addr_len;
  base::ScopedFD fd(
      MakeUnixAddrForPath(socket_name, &unix_addr, &unix_addr_len));
  if (!fd.is_valid())
    return false;

  // Make sure the path we need exists.
  if (!base::CreateDirectory(socket_dir)) {
    LOG(ERROR) << "Couldn't create directory: " << socket_dir.value();
    return false;
  }

  // Delete any old FS instances.
  if (unlink(socket_name.c_str()) < 0 && errno != ENOENT) {
    PLOG(ERROR) << "unlink " << socket_name;
    return false;
  }

  // Bind the socket.
  if (bind(fd.get(), reinterpret_cast<const sockaddr*>(&unix_addr),
           unix_addr_len) < 0) {
    PLOG(ERROR) << "bind " << socket_path.value();
    return false;
  }

  // Start listening on the socket.
  if (listen(fd.get(), SOMAXCONN) < 0) {
    PLOG(ERROR) << "listen " << socket_path.value();
    unlink(socket_name.c_str());
    return false;
  }

  *server_listen_fd = fd.release();
  return true;
}

bool IsRecoverableError(int err) {
  return errno == ECONNABORTED || errno == EMFILE || errno == ENFILE ||
         errno == ENOMEM || errno == ENOBUFS;
}

bool ServerAcceptConnection(int server_listen_fd, int* server_socket) {
  DCHECK(server_socket);
  *server_socket = -1;

  base::ScopedFD accept_fd(HANDLE_EINTR(accept(server_listen_fd, NULL, 0)));
  if (!accept_fd.is_valid())
    return IsRecoverableError(errno);
  if (HANDLE_EINTR(fcntl(accept_fd.get(), F_SETFL, O_NONBLOCK)) < 0) {
    PLOG(ERROR) << "fcntl(O_NONBLOCK) " << accept_fd.get();
    // It's safe to keep listening on |server_listen_fd| even if the attempt to
    // set O_NONBLOCK failed on the client fd.
    return true;
  }

  *server_socket = accept_fd.release();
  return true;
}

}  // namespace internal
