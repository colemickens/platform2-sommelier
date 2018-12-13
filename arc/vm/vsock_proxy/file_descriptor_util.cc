// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "arc/vm/vsock_proxy/file_descriptor_util.h"

#include <fcntl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <base/logging.h>

namespace arc {

base::Optional<std::pair<base::ScopedFD, base::ScopedFD>> CreatePipe() {
  int fds[2];
  if (pipe2(fds, O_CLOEXEC) == -1) {
    PLOG(ERROR) << "Failed to create pipe";
    return base::nullopt;
  }

  return base::make_optional(
      std::make_pair(base::ScopedFD(fds[0]), base::ScopedFD(fds[1])));
}

base::Optional<std::pair<base::ScopedFD, base::ScopedFD>> CreateSocketPair() {
  int fds[2];
  if (socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0 /* protocol */, fds) ==
      -1) {
    PLOG(ERROR) << "Failed to create socketpair";
    return base::nullopt;
  }

  return base::make_optional(
      std::make_pair(base::ScopedFD(fds[0]), base::ScopedFD(fds[1])));
}

}  // namespace arc
