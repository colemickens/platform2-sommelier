/*
 * Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "arc_camera3_service_provider.h"
#include "ipc_util.h"

#include <fcntl.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/files/scoped_file.h>
#include <base/logging.h>

namespace arc {

const char kArcCameraSocketPath[] = "/var/run/camera/camera3.sock";

ArcCamera3ServiceProvider::ArcCamera3ServiceProvider() {
  // Reap zombie processes when child process exited.
  signal(SIGCHLD, SIG_IGN);
}

ArcCamera3ServiceProvider::~ArcCamera3ServiceProvider() {}

int ArcCamera3ServiceProvider::Start() {
  base::FilePath socket_path(kArcCameraSocketPath);

  // Set file permission to 0660.
  // Container accesses the socket file by using arc-camera group.
  umask(0117);

  int raw_fd = -1;
  if (!::internal::CreateServerUnixDomainSocket(socket_path, &raw_fd)) {
    LOG(ERROR) << "CreateSreverUnixDomainSocket failed";
    return -1;
  }
  base::ScopedFD socket_fd(raw_fd);

  // Make socket blocking.
  int flags = HANDLE_EINTR(fcntl(socket_fd.get(), F_GETFL));
  if (flags == -1) {
    PLOG(ERROR) << "fcntl(F_GETFL)";
    return -1;
  }
  if (HANDLE_EINTR(fcntl(socket_fd.get(), F_SETFL, flags & ~O_NONBLOCK)) ==
      -1) {
    PLOG(ERROR) << "fcntl(F_SETFL) failed to disable O_NONBLOCK";
    return -1;
  }

  while (1) {
    int accept_fd = -1;
    if (!::internal::ServerAcceptConnection(socket_fd.get(), &accept_fd)) {
      PLOG(ERROR) << "Accept failed";
      break;
    }
    if (accept_fd < 0) {
      LOG(ERROR) << "Invalid accept fd: " << accept_fd;
    } else {
      VLOG(1) << "Accepted a client, fd: " << accept_fd;
      pid_t child_pid = fork();
      if (child_pid < 0) {
        PLOG(ERROR) << "Fork failed";
        continue;
      }

      if (child_pid == 0) {  // child
        return accept_fd;
      } else {  // parent
        close(accept_fd);
      }
    }
  }
  return -1;
}

}  // namespace arc
