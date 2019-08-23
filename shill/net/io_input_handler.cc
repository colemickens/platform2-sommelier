// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/net/io_input_handler.h"

#include <string>
#include <unistd.h>

#include <base/logging.h>
#include <base/strings/stringprintf.h>

namespace shill {

IOInputHandler::IOInputHandler(int fd,
                               const InputCallback& input_callback,
                               const ErrorCallback& error_callback)
    : fd_(fd),
      fd_watcher_(FROM_HERE),
      input_callback_(input_callback),
      error_callback_(error_callback) {}

IOInputHandler::~IOInputHandler() {
  Stop();
}

void IOInputHandler::Start() {
  if (!base::MessageLoopForIO::current()->WatchFileDescriptor(
          fd_, true, base::MessageLoopForIO::WATCH_READ, &fd_watcher_, this)) {
    LOG(ERROR) << "WatchFileDescriptor failed on read";
  }
}

void IOInputHandler::Stop() {
  fd_watcher_.StopWatchingFileDescriptor();
}

void IOInputHandler::OnFileCanReadWithoutBlocking(int fd) {
  CHECK_EQ(fd_, fd);

  unsigned char buf[IOHandler::kDataBufferSize];
  ssize_t len = read(fd, buf, sizeof(buf));
  if (len < 0) {
    std::string condition = base::StringPrintf("File read error: %d", errno);
    LOG(ERROR) << condition;
    error_callback_.Run(condition);
  } else {
    InputData input_data(buf, len);
    input_callback_.Run(&input_data);
  }
}

void IOInputHandler::OnFileCanWriteWithoutBlocking(int fd) {
  NOTREACHED() << "Not watching file descriptor for write";
}

}  // namespace shill
