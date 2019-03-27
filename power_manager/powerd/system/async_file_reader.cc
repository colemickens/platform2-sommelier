// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/powerd/system/async_file_reader.h"

#include <fcntl.h>
#include <sys/stat.h>

#include <cerrno>
#include <cstdio>

#include <base/logging.h>

#include "power_manager/common/util.h"

namespace power_manager {
namespace system {

namespace {

// Since we don't know the file size in advance, we'll have to read successively
// larger chunks.  Start with 4 KB and double the chunk size with each new read.
const size_t kInitialFileReadSize = 4096;

// How often to poll for the AIO status.
const int kPollMs = 100;

}  // namespace

AsyncFileReader::AsyncFileReader()
    : read_in_progress_(false),
      fd_(-1),
      initial_read_size_(kInitialFileReadSize) {}

AsyncFileReader::~AsyncFileReader() {
  Reset();
  close(fd_);
}

bool AsyncFileReader::Init(const base::FilePath& path) {
  CHECK_EQ(fd_, -1) << "Attempting to open new file when a valid file "
                    << "descriptor exists.";
  fd_ = open(path.value().c_str(), O_RDONLY, 0);
  if (fd_ == -1) {
    PLOG(ERROR) << "Could not open file " << path.value();
    return false;
  }
  path_ = path;
  return true;
}

bool AsyncFileReader::HasOpenedFile() const {
  return (fd_ != -1);
}

void AsyncFileReader::StartRead(
    const base::Callback<void(const std::string&)>& read_cb,
    const base::Callback<void()>& error_cb) {
  Reset();

  if (fd_ == -1) {
    LOG(ERROR) << "No file handle available.";
    if (!error_cb.is_null())
      error_cb.Run();
    return;
  }

  if (!AsyncRead(initial_read_size_, 0)) {
    if (!error_cb.is_null())
      error_cb.Run();
    return;
  }
  read_cb_ = read_cb;
  error_cb_ = error_cb;
  read_in_progress_ = true;
}

void AsyncFileReader::UpdateState() {
  if (!read_in_progress_) {
    update_state_timer_.Reset();
    return;
  }

  int status = aio_error(&aio_control_);

  // If the read is still in-progress, keep the timer running.
  if (status == EINPROGRESS)
    return;

  // Otherwise, we stop the timer.
  update_state_timer_.Stop();

  switch (status) {
    case ECANCELED:
      Reset();
      break;
    case 0: {
      size_t size = aio_return(&aio_control_);
      // Save the data that was read, and free the buffer.
      stored_data_.insert(stored_data_.end(), aio_buffer_.get(),
                          aio_buffer_.get() + size);
      aio_buffer_ = nullptr;

      if (size == aio_control_.aio_nbytes) {
        // Read more data if the previous read didn't reach the end of file.
        if (AsyncRead(size * 2, aio_control_.aio_offset + size))
          break;
      }
      if (!read_cb_.is_null())
        read_cb_.Run(stored_data_);
      Reset();
      break;
    }
    default: {
      LOG(ERROR) << "Error during read of file " << path_.value()
                 << ", status=" << status;
      if (!error_cb_.is_null())
        error_cb_.Run();
      Reset();
      break;
    }
  }
}

void AsyncFileReader::Reset() {
  if (!read_in_progress_)
    return;

  update_state_timer_.Stop();

  int cancel_result = aio_cancel(fd_, &aio_control_);
  if (cancel_result == -1) {
    PLOG(ERROR) << "aio_cancel() failed";
  } else if (cancel_result == AIO_NOTCANCELED) {
    LOG(INFO) << "aio_cancel() returned AIO_NOTCANCELED; waiting for "
              << "request to complete";
    const aiocb* aiocb_list = {&aio_control_};
    if (aio_suspend(&aiocb_list, 1, nullptr) == -1)
      PLOG(ERROR) << "aio_suspend() failed";
  }

  aio_buffer_ = nullptr;
  stored_data_.clear();
  read_cb_.Reset();
  error_cb_.Reset();
  read_in_progress_ = false;
}

bool AsyncFileReader::AsyncRead(int size, int offset) {
  aio_buffer_.reset(new char[size]);

  memset(&aio_control_, 0, sizeof(aio_control_));
  aio_control_.aio_nbytes = size;
  aio_control_.aio_fildes = fd_;
  aio_control_.aio_offset = offset;
  aio_control_.aio_buf = aio_buffer_.get();

  if (aio_read(&aio_control_) == -1) {
    LOG(ERROR) << "Unable to access " << path_.value();
    aio_buffer_ = nullptr;
    return false;
  }

  update_state_timer_.Start(FROM_HERE,
                            base::TimeDelta::FromMilliseconds(kPollMs), this,
                            &AsyncFileReader::UpdateState);
  return true;
}

}  // namespace system
}  // namespace power_manager
