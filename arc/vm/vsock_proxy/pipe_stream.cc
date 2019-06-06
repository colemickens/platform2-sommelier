// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "arc/vm/vsock_proxy/pipe_stream.h"

#include <errno.h>
#include <unistd.h>

#include <string>
#include <utility>

#include <base/files/file_util.h>
#include <base/logging.h>
#include <base/posix/eintr_wrapper.h>

namespace arc {

PipeStream::PipeStream(base::ScopedFD pipe_fd) : pipe_fd_(std::move(pipe_fd)) {}

PipeStream::~PipeStream() = default;

StreamBase::ReadResult PipeStream::Read() {
  std::string buf;
  buf.resize(4096);
  ssize_t size = HANDLE_EINTR(read(pipe_fd_.get(), &buf[0], buf.size()));
  if (size < 0) {
    int error_code = errno;
    PLOG(ERROR) << "Failed to read";
    return {error_code, std::string(), {}};
  }

  buf.resize(size);
  return {0, std::move(buf), {}};
}

bool PipeStream::Write(std::string blob, std::vector<base::ScopedFD> fds) {
  if (!fds.empty()) {
    LOG(ERROR) << "Cannot write file descriptors.";
    return false;
  }

  // WriteFileDescriptor takes care of the short write.
  if (!base::WriteFileDescriptor(pipe_fd_.get(), blob.data(), blob.size())) {
    PLOG(ERROR) << "Failed to write";
    return false;
  }
  return true;
}

bool PipeStream::Pread(uint64_t count,
                       uint64_t offset,
                       arc_proxy::PreadResponse* response) {
  LOG(ERROR) << "Pread for pipe file descriptor is unsupported.";
  return false;
}

bool PipeStream::Fstat(arc_proxy::FstatResponse* response) {
  LOG(ERROR) << "Fstat for pipe file descriptor is unsupported.";
  return false;
}

}  // namespace arc
