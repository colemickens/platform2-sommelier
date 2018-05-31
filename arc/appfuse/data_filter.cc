// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "arc/appfuse/data_filter.h"

#include <sys/socket.h>

#include <utility>

#include <base/bind.h>
#include <base/files/file_util.h>
#include <base/posix/eintr_wrapper.h>

namespace arc {
namespace appfuse {

namespace {

// This must be larger than kFuseMaxWrite and kFuseMaxRead defined in
// Android's system/core/libappfuse/include/libappfuse/FuseBuffer.h.
constexpr size_t kMaxFuseDataSize = 256 * 1024;

}  // namespace

DataFilter::DataFilter() : watch_thread_("DataFilter") {}

DataFilter::~DataFilter() {
  watch_thread_.Stop();
}

base::ScopedFD DataFilter::Start(base::ScopedFD fd_dev) {
  int raw_socks[2];
  // SOCK_SEQPACKET to mimic the behavior of real /dev/fuse whose read & write
  // result always contains one single command.
  if (socketpair(AF_UNIX, SOCK_SEQPACKET, 0, raw_socks) == -1) {
    PLOG(ERROR) << "socketpair() failed.";
    return base::ScopedFD();
  }
  base::ScopedFD socket_for_filter(raw_socks[0]);
  base::ScopedFD socket_for_app(raw_socks[1]);

  base::Thread::Options options;
  options.message_loop_type = base::MessageLoop::TYPE_IO;
  if (!watch_thread_.StartWithOptions(options)) {
    LOG(ERROR) << "Failed to start a data filter thread.";
    return base::ScopedFD();
  }
  fd_dev_ = std::move(fd_dev);
  fd_socket_ = std::move(socket_for_filter);
  // Unretained(this) here is safe because watch_thread_ is owned by |this|.
  watch_thread_.task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&DataFilter::StartWatching, base::Unretained(this)));
  return socket_for_app;
}

void DataFilter::OnFileCanReadWithoutBlocking(int fd) {
  std::vector<char> buf(kMaxFuseDataSize);
  int result = HANDLE_EINTR(read(fd, buf.data(), buf.size()));
  if (result <= 0) {
    if (result < 0) {
      PLOG(ERROR) << "Failed to read "
                  << (IsDevFuseFD(fd) ? "/dev/fuse" : "socket");
    } else if (IsDevFuseFD(fd)) {
      LOG(ERROR) << "Unexpected EOF on /dev/fuse";
    }
    AbortWatching();
    return;
  }
  buf.resize(result);

  // TODO(hashimoto): Check the contents of |buf| to reject unexpected commands.
  if (IsDevFuseFD(fd)) {
    pending_data_to_socket_.push_back(std::move(buf));
  } else {
    pending_data_to_dev_.push_back(std::move(buf));
  }
}

void DataFilter::OnFileCanWriteWithoutBlocking(int fd) {
  auto* pending_data =
      IsDevFuseFD(fd) ? &pending_data_to_dev_ : &pending_data_to_socket_;

  if (pending_data->empty())  // Nothing to do.
    return;

  std::vector<char> buf = std::move(pending_data->front());
  pending_data->pop_front();
  int result = HANDLE_EINTR(write(fd, buf.data(), buf.size()));
  if (result != buf.size()) {
    if (result < 0) {
      PLOG(ERROR) << "Failed to write to "
                  << (IsDevFuseFD(fd) ? "/dev/fuse" : "socket");
    } else {
      // Partial write should never happen with /dev/fuse nor sockets.
      LOG(ERROR) << "Unexpected write result " << result << " when writing to "
                 << (IsDevFuseFD(fd) ? "/dev/fuse" : "socket");
    }
    AbortWatching();
    return;
  }
}

void DataFilter::StartWatching() {
  CHECK(base::MessageLoopForIO::current()->WatchFileDescriptor(
      fd_dev_.get(), true, base::MessageLoopForIO::WATCH_READ_WRITE,
      &watcher_dev_, this));
  CHECK(base::MessageLoopForIO::current()->WatchFileDescriptor(
      fd_socket_.get(), true, base::MessageLoopForIO::WATCH_READ_WRITE,
      &watcher_socket_, this));
}

void DataFilter::AbortWatching() {
  watcher_dev_.StopWatchingFileDescriptor();
  watcher_socket_.StopWatchingFileDescriptor();
  fd_dev_.reset();
  fd_socket_.reset();
}

}  // namespace appfuse
}  // namespace arc
