// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "login_manager/ipc_channel.h"

#include <errno.h>

#include <base/logging.h>

namespace login_manager {

bool IpcChannel::DoInit(const char pipe_name[], const char mode[], uid_t uid) {
  CHECK(pipe_name);
  // Create the FIFO if it does not exist
  umask(0);
  // TODO(cmasone): Can we do some kind of setuid here?
  mknod(pipe_name, S_IFIFO|0640, 0);
  chown(pipe_name, uid, uid);
  pipe_ = fopen(pipe_name, mode);
  if (!pipe_) {
    LOG(ERROR) << "Couldn't open pipe: " << strerror(errno);
    return false;
  }
  clearerr(pipe_);
  return true;
}

IpcReadChannel::IpcReadChannel(const std::string& pipe_name)
    : pipe_name_(pipe_name),
      uid_(getuid()) {
}

IpcReadChannel::IpcReadChannel(const std::string& pipe_name, const uid_t uid)
    : pipe_name_(pipe_name),
      uid_(uid) {
}

IpcReadChannel::~IpcReadChannel() {
}

IpcWriteChannel::IpcWriteChannel(const std::string& pipe_name)
    : pipe_name_(pipe_name),
      uid_(getuid()) {
}

IpcWriteChannel::IpcWriteChannel(const std::string& pipe_name, const uid_t uid)
    : pipe_name_(pipe_name),
      uid_(uid) {
}

IpcWriteChannel::~IpcWriteChannel() {
}

}  // namespace login_manager
