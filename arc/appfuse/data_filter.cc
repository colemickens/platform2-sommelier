// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "arc/appfuse/data_filter.h"

#include <linux/fuse.h>
#include <sys/socket.h>
#include <sys/stat.h>

#include <utility>

#include <base/bind.h>
#include <base/posix/eintr_wrapper.h>
#include <base/threading/thread_task_runner_handle.h>

namespace arc {
namespace appfuse {

namespace {

// This must be larger than kFuseMaxWrite and kFuseMaxRead defined in
// Android's system/core/libappfuse/include/libappfuse/FuseBuffer.h.
constexpr size_t kMaxFuseDataSize = 256 * 1024;

}  // namespace

DataFilter::DataFilter()
    : watch_thread_("DataFilter"),
      watcher_dev_(FROM_HERE),
      watcher_socket_(FROM_HERE),
      origin_task_runner_(base::ThreadTaskRunnerHandle::Get()) {}

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

  if (IsDevFuseFD(fd)) {
    if (!FilterDataFromDev(&buf))
      AbortWatching();
  } else {
    if (!FilterDataFromSocket(&buf))
      AbortWatching();
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

  if (!on_stopped_callback_.is_null())
    origin_task_runner_->PostTask(FROM_HERE, on_stopped_callback_);
}

bool DataFilter::FilterDataFromDev(std::vector<char>* data) {
  const auto* header = reinterpret_cast<const fuse_in_header*>(data->data());
  if (data->size() < sizeof(fuse_in_header) || header->len != data->size()) {
    LOG(ERROR) << "Invalid fuse_in_header";
    return false;
  }
  switch (header->opcode) {
    case FUSE_FORGET:  // No response for FORGET, so no need to save opcode.
      break;
    case FUSE_LOOKUP:
    case FUSE_GETATTR:
    case FUSE_OPEN:
    case FUSE_READ:
    case FUSE_WRITE:
    case FUSE_RELEASE:
    case FUSE_FSYNC:
    case FUSE_INIT: {
      // Save opcode to verify the response later.
      if (unique_to_opcode_.count(header->unique)) {
        LOG(ERROR) << "Conflicting unique value";
        return false;
      }
      unique_to_opcode_[header->unique] = header->opcode;
      break;
    }
    default: {
      // Operation not supported. Return ENOSYS to /dev/fuse.
      std::vector<char> response(sizeof(fuse_out_header));
      auto* out_header = reinterpret_cast<fuse_out_header*>(response.data());
      out_header->len = sizeof(fuse_out_header);
      out_header->error = -ENOSYS;
      out_header->unique = header->unique;
      pending_data_to_dev_.push_back(std::move(response));
      return true;
    }
  }
  // Pass the data to the socket.
  pending_data_to_socket_.push_back(std::move(*data));
  return true;
}

bool DataFilter::FilterDataFromSocket(std::vector<char>* data) {
  const auto* header = reinterpret_cast<const fuse_out_header*>(data->data());
  if (data->size() < sizeof(fuse_out_header) || header->len != data->size()) {
    LOG(ERROR) << "Invalid fuse_out_header";
    return false;
  }
  // Get opcode of the original request.
  auto it = unique_to_opcode_.find(header->unique);
  if (it == unique_to_opcode_.end()) {
    LOG(ERROR) << "Invalid unique value";
    return false;
  }
  const int opcode = it->second;
  unique_to_opcode_.erase(it);

  if (header->error == 0) {
    // Check the response contents.
    switch (opcode) {
      case FUSE_LOOKUP: {
        if (data->size() < sizeof(fuse_out_header) + sizeof(fuse_entry_out)) {
          LOG(ERROR) << "Invalid LOOKUP response";
          return false;
        }
        const auto* entry_out = reinterpret_cast<const fuse_entry_out*>(
            data->data() + sizeof(fuse_out_header));
        if (!S_ISREG(entry_out->attr.mode) && !S_ISDIR(entry_out->attr.mode)) {
          LOG(ERROR) << "Invalid mode";
          return false;
        }
        break;
      }
      case FUSE_GETATTR: {
        if (data->size() < sizeof(fuse_out_header) + sizeof(fuse_attr_out)) {
          LOG(ERROR) << "Invalid GETATTR response";
          return false;
        }
        const auto* attr_out = reinterpret_cast<const fuse_attr_out*>(
            data->data() + sizeof(fuse_out_header));
        if (!S_ISREG(attr_out->attr.mode) && !S_ISDIR(attr_out->attr.mode)) {
          LOG(ERROR) << "Invalid mode";
          return false;
        }
        break;
      }
    }
  }
  // Pass the data to /dev/fuse.
  pending_data_to_dev_.push_back(std::move(*data));
  return true;
}

}  // namespace appfuse
}  // namespace arc
