// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/net/io_ready_handler.h"

#include <unistd.h>

#include <base/logging.h>

namespace shill {

IOReadyHandler::IOReadyHandler(int fd,
                               ReadyMode mode,
                               const ReadyCallback& ready_callback)
    : fd_(fd),
      fd_watcher_(FROM_HERE),
      ready_mode_(mode),
      ready_callback_(ready_callback) {}

IOReadyHandler::~IOReadyHandler() {
  Stop();
}

void IOReadyHandler::Start() {
  CHECK(ready_mode_ == kModeOutput || ready_mode_ == kModeInput);
  base::MessageLoopForIO::Mode mode;
  if (ready_mode_ == kModeOutput) {
    mode = base::MessageLoopForIO::WATCH_WRITE;
  } else {
    mode = base::MessageLoopForIO::WATCH_READ;
  }

  if (!base::MessageLoopForIO::current()->WatchFileDescriptor(
          fd_, true, mode, &fd_watcher_, this)) {
    LOG(ERROR) << "WatchFileDescriptor failed on read";
  }
}

void IOReadyHandler::Stop() {
  fd_watcher_.StopWatchingFileDescriptor();
}

void IOReadyHandler::OnFileCanReadWithoutBlocking(int fd) {
  CHECK_EQ(fd_, fd);
  CHECK_EQ(ready_mode_, kModeInput);

  ready_callback_.Run(fd_);
}

void IOReadyHandler::OnFileCanWriteWithoutBlocking(int fd) {
  CHECK_EQ(fd_, fd);
  CHECK_EQ(ready_mode_, kModeOutput);

  ready_callback_.Run(fd_);
}

}  // namespace shill
