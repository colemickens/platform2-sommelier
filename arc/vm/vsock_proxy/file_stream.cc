// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "arc/vm/vsock_proxy/file_stream.h"

#include <errno.h>

#include <string>
#include <utility>

#include <base/logging.h>
#include <base/posix/eintr_wrapper.h>

#include "arc/vm/vsock_proxy/message.pb.h"

namespace arc {

FileStream::FileStream(base::ScopedFD file_fd) : file_fd_(std::move(file_fd)) {}

FileStream::~FileStream() = default;

bool FileStream::Read(arc_proxy::VSockMessage* message) {
  LOG(ERROR) << "FileStream::Read is unsupported.";
  return false;
}

bool FileStream::Write(arc_proxy::Data* data) {
  LOG(ERROR) << "FileStream::Write is unsupported.";
  return false;
}

bool FileStream::Pread(uint64_t count,
                       uint64_t offset,
                       arc_proxy::PreadResponse* response) {
  std::string buffer;
  buffer.resize(count);
  int result = HANDLE_EINTR(pread(file_fd_.get(), &buffer[0], count, offset));
  if (result < 0) {
    response->set_error_code(errno);
  } else {
    buffer.resize(result);
    response->set_error_code(0);
    *response->mutable_blob() = std::move(buffer);
  }
  return true;
}

}  // namespace arc
