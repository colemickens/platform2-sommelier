/*
 * Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "cros-camera/ipc_util.h"

#include <errno.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <base/files/file_util.h>
#include <mojo/edk/embedder/embedder.h>
#include <mojo/edk/embedder/platform_channel_pair.h>
#include <mojo/edk/embedder/platform_channel_utils_posix.h>
#include <mojo/edk/embedder/platform_handle_vector.h>
#include <mojo/edk/embedder/scoped_platform_handle.h>

#include "cros-camera/common.h"

namespace cros {

namespace {

// The following four functions were taken from
// ipc/unix_domain_socket_util.{h,cc}.

static const size_t kMaxSocketNameLength = 104;

bool CreateUnixDomainSocket(base::ScopedFD* out_fd) {
  DCHECK(out_fd);

  // Create the unix domain socket.
  base::ScopedFD fd(socket(AF_UNIX, SOCK_STREAM, 0));
  if (!fd.is_valid()) {
    PLOGF(ERROR) << "Failed to create AF_UNIX socket:";
    return false;
  }

  // Now set it as non-blocking.
  if (!base::SetNonBlocking(fd.get())) {
    PLOGF(ERROR) << "base::SetNonBlocking() on " << fd.get() << " failed:";
    return false;
  }

  fd.swap(*out_fd);

  return true;
}

bool MakeUnixAddrForPath(const std::string& socket_name,
                         struct sockaddr_un* unix_addr,
                         size_t* unix_addr_len) {
  DCHECK(unix_addr);
  DCHECK(unix_addr_len);

  if (socket_name.length() == 0) {
    LOGF(ERROR) << "Empty socket name provided for unix socket address.";
    return false;
  }
  // We reject socket_name.length() == kMaxSocketNameLength to make room for
  // the NUL terminator at the end of the string.
  if (socket_name.length() >= kMaxSocketNameLength) {
    LOGF(ERROR) << "Socket name too long: " << socket_name;
    return false;
  }

  // Create unix_addr structure.
  memset(unix_addr, 0, sizeof(struct sockaddr_un));
  unix_addr->sun_family = AF_UNIX;
  strncpy(unix_addr->sun_path, socket_name.c_str(), kMaxSocketNameLength);
  *unix_addr_len =
      offsetof(struct sockaddr_un, sun_path) + socket_name.length();
  return true;
}

bool IsRecoverableError(int err) {
  return errno == ECONNABORTED || errno == EMFILE || errno == ENFILE ||
         errno == ENOMEM || errno == ENOBUFS;
}

}  // namespace

bool CreateServerUnixDomainSocket(const base::FilePath& socket_path,
                                  int* server_listen_fd) {
  DCHECK(server_listen_fd);

  std::string socket_name = socket_path.value();
  base::FilePath socket_dir = socket_path.DirName();

  struct sockaddr_un unix_addr;
  size_t unix_addr_len;
  if (!MakeUnixAddrForPath(socket_name, &unix_addr, &unix_addr_len)) {
    return false;
  }

  base::ScopedFD fd;
  if (!CreateUnixDomainSocket(&fd)) {
    return false;
  }

  // Make sure the path we need exists.
  if (!base::CreateDirectory(socket_dir)) {
    LOGF(ERROR) << "Couldn't create directory: " << socket_dir.value();
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

base::ScopedFD CreateClientUnixDomainSocket(const base::FilePath& socket_path) {
  struct sockaddr_un unix_addr;
  size_t unix_addr_len;
  if (!MakeUnixAddrForPath(socket_path.value(), &unix_addr, &unix_addr_len))
    return base::ScopedFD();

  base::ScopedFD fd;
  if (!CreateUnixDomainSocket(&fd))
    return base::ScopedFD();

  if (HANDLE_EINTR(connect(fd.get(), reinterpret_cast<sockaddr*>(&unix_addr),
                           unix_addr_len)) < 0) {
    PLOGF(ERROR) << "connect " << socket_path.value();
    return base::ScopedFD();
  }

  return fd;
}

MojoResult CreateMojoChannelToParentByUnixDomainSocket(
    const base::FilePath& socket_path,
    mojo::ScopedMessagePipeHandle* child_pipe) {
  base::ScopedFD client_socket_fd = CreateClientUnixDomainSocket(socket_path);
  if (!client_socket_fd.is_valid()) {
    LOGF(WARNING) << "Failed to connect to " << socket_path.value();
    return MOJO_RESULT_INTERNAL;
  }
  mojo::edk::ScopedPlatformHandle socketHandle(
      mojo::edk::PlatformHandle(client_socket_fd.release()));

  // Set socket to blocking
  int flags = HANDLE_EINTR(fcntl(socketHandle.get().handle, F_GETFL));
  if (flags == -1) {
    PLOGF(ERROR) << "fcntl(F_GETFL) failed:";
    return MOJO_RESULT_INTERNAL;
  }
  if (HANDLE_EINTR(fcntl(socketHandle.get().handle, F_SETFL,
                         flags & ~O_NONBLOCK)) == -1) {
    PLOGF(ERROR) << "fcntl(F_SETFL) failed:";
    return MOJO_RESULT_INTERNAL;
  }

  const int kTokenSize = 32;
  char token[kTokenSize] = {};
  std::deque<mojo::edk::PlatformHandle> platformHandles;
  mojo::edk::PlatformChannelRecvmsg(socketHandle.get(), token, sizeof(token),
                                    &platformHandles, true);
  if (platformHandles.size() != 1) {
    LOGF(ERROR) << "Unexpected number of handles received, expected 1: "
                << platformHandles.size();
    return MOJO_RESULT_INTERNAL;
  }
  mojo::edk::ScopedPlatformHandle parent_pipe(platformHandles.back());
  platformHandles.pop_back();
  if (!parent_pipe.is_valid()) {
    LOGF(ERROR) << "Invalid parent pipe";
    return MOJO_RESULT_INTERNAL;
  }
  mojo::edk::SetParentPipeHandle(std::move(parent_pipe));

  *child_pipe =
      mojo::edk::CreateChildMessagePipe(std::string(token, kTokenSize));

  return MOJO_RESULT_OK;
}

MojoResult CreateMojoChannelToChildByUnixDomainSocket(
    const base::FilePath& socket_path,
    mojo::ScopedMessagePipeHandle* parent_pipe) {
  base::ScopedFD client_socket_fd = CreateClientUnixDomainSocket(socket_path);
  if (!client_socket_fd.is_valid()) {
    LOGF(WARNING) << "Failed to connect to " << socket_path.value();
    return MOJO_RESULT_INTERNAL;
  }

  VLOGF(1) << "Setting up message pipe";
  mojo::edk::PlatformChannelPair channel_pair;
  const int kUnusedProcessHandle = 0;
  mojo::edk::ChildProcessLaunched(kUnusedProcessHandle,
                                  channel_pair.PassServerHandle());
  mojo::edk::ScopedPlatformHandleVectorPtr handles(
      new mojo::edk::PlatformHandleVector(
          {channel_pair.PassClientHandle().release()}));
  std::string token = mojo::edk::GenerateRandomToken();
  VLOGF(1) << "Generated token: " << token;
  struct iovec iov = {const_cast<char*>(token.c_str()), token.length()};
  if (mojo::edk::PlatformChannelSendmsgWithHandles(
          mojo::edk::PlatformHandle(client_socket_fd.get()), &iov, 1,
          handles->data(), handles->size()) == -1) {
    LOGF(ERROR) << "Failed to send token and handle: " << strerror(errno);
    return MOJO_RESULT_INTERNAL;
  }

  *parent_pipe = mojo::edk::CreateParentMessagePipe(token);
  return MOJO_RESULT_OK;
}

mojo::ScopedHandle WrapPlatformHandle(int handle) {
  MojoHandle wrapped_handle;
  MojoResult wrap_result = mojo::edk::CreatePlatformHandleWrapper(
      mojo::edk::ScopedPlatformHandle(mojo::edk::PlatformHandle(handle)),
      &wrapped_handle);
  if (wrap_result != MOJO_RESULT_OK) {
    LOGF(ERROR) << "Failed to wrap platform handle: " << wrap_result;
    return mojo::ScopedHandle(mojo::Handle());
  }
  return mojo::ScopedHandle(mojo::Handle(wrapped_handle));
}

// Transfers ownership of the handle.
int UnwrapPlatformHandle(mojo::ScopedHandle handle) {
  mojo::edk::ScopedPlatformHandle scoped_platform_handle;
  MojoResult mojo_result = mojo::edk::PassWrappedPlatformHandle(
      handle.release().value(), &scoped_platform_handle);
  if (mojo_result != MOJO_RESULT_OK) {
    LOGF(ERROR) << "Failed to unwrap handle: " << mojo_result;
    return -EINVAL;
  }
  return scoped_platform_handle.release().handle;
}

}  // namespace cros
