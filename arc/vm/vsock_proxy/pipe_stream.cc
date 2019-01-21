// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "arc/vm/vsock_proxy/pipe_stream.h"

#include <unistd.h>

#include <utility>

#include <base/files/file_util.h>
#include <base/logging.h>
#include <base/posix/eintr_wrapper.h>

namespace arc {

PipeStream::PipeStream(base::ScopedFD pipe_fd) : pipe_fd_(std::move(pipe_fd)) {}

PipeStream::~PipeStream() = default;

bool PipeStream::Read(arc_proxy::Message* message) {
  char buf[4096];
  ssize_t size = HANDLE_EINTR(read(pipe_fd_.get(), buf, sizeof(buf)));
  if (size < 0) {
    PLOG(ERROR) << "Failed to read";
    return false;
  }

  message->set_data(buf, size);
  return true;
}

bool PipeStream::Write(const arc_proxy::Message& message) {
  // WriteFileDescriptor takes care of the short write.
  if (!base::WriteFileDescriptor(pipe_fd_.get(), message.data().data(),
                                 message.data().size())) {
    PLOG(ERROR) << "Failed to write";
    return false;
  }
  return true;
}

}  // namespace arc
