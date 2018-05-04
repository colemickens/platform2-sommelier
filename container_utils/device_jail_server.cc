// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "container_utils/device_jail_server.h"

#include <fcntl.h>
#include <sys/types.h>
#include <unistd.h>

#include <utility>
#include <vector>

#include <base/logging.h>
#include <base/posix/eintr_wrapper.h>

namespace {

const char kJailRequestPath[] = "/dev/jail-request";

}  // namespace

namespace device_jail {

DeviceJailServer::DeviceJailServer(std::unique_ptr<Delegate> delegate,
                                   base::ScopedFD fd)
    : delegate_(std::move(delegate)),
      fd_(std::move(fd)),
      watcher_(FROM_HERE) {}

// static
std::unique_ptr<DeviceJailServer> DeviceJailServer::CreateAndListen(
    std::unique_ptr<DeviceJailServer::Delegate> delegate,
    base::MessageLoopForIO* message_loop) {
  if (!delegate || !message_loop)
    return std::unique_ptr<DeviceJailServer>();

  base::ScopedFD fd(HANDLE_EINTR(open(kJailRequestPath, O_RDWR)));
  if (!fd.is_valid()) {
    PLOG(ERROR) << "could not open jail request device";
    return std::unique_ptr<DeviceJailServer>();
  }

  std::unique_ptr<DeviceJailServer> server(
      new DeviceJailServer(std::move(delegate), std::move(fd)));
  server->Start(message_loop);
  return server;
}

void DeviceJailServer::Start(base::MessageLoopForIO* message_loop) {
  message_loop->WatchFileDescriptor(
      fd_.get(), true, base::MessageLoopForIO::WATCH_READ, &watcher_, this);
}

DeviceJailServer::~DeviceJailServer() {
  watcher_.StopWatchingFileDescriptor();
}

void DeviceJailServer::OnFileCanReadWithoutBlocking(int fd) {
  CHECK_EQ(fd, fd_.get());

  std::vector<char> buf(PATH_MAX);
  ssize_t ret = TEMP_FAILURE_RETRY(read(fd, buf.data(), PATH_MAX));
  if (ret < 0) {
    PLOG(ERROR) << "Failed to read from jail request device";
    return;
  }

  std::string path(buf.data(), buf.data() + ret);
  jail_request_result result = delegate_->HandleRequest(path);
  ret = write(fd, &result, sizeof(result));
  if (ret < 0)
    PLOG(ERROR) << "Failed to write to jail request device";
}

}  // namespace device_jail
