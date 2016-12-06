// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "authpolicy/pipe_helper.h"

#include <fcntl.h>

#include <algorithm>

#include <base/files/file_util.h>

namespace authpolicy {
namespace helper {

namespace {

// The maximum number of bytes to get from a child process
// (from stdout and stderr).
const size_t kMaxReadSize = 256 * 1024;  // 256 Kb
// The size of the buffer used to fetch stdout and stderr.
const size_t kBufferSize = 1024;  // 1 Kb

}  // namespace

bool ReadPipeToString(int fd, std::string* out) {
  char buffer[kBufferSize];
  size_t total_read = 0;
  while (total_read < kMaxReadSize) {
    const ssize_t bytes_read = HANDLE_EINTR(
        read(fd, buffer, std::min(kBufferSize, kMaxReadSize - total_read)));
    if (bytes_read < 0)
      return false;
    if (bytes_read == 0)
      return true;
    total_read += bytes_read;
    out->append(buffer, bytes_read);
  }

  // Size limit hit. Do one more read to check if the file size is exactly
  // kMaxReadSize bytes.
  return HANDLE_EINTR(read(fd, buffer, 1)) == 0;
}

bool WriteStringToPipe(const std::string& str, int fd) {
  return base::WriteFileDescriptor(fd, str.c_str(), str.size());
}

bool CopyPipe(int fd_in, int fd_out) {
  return tee(fd_in, fd_out, INT_MAX, 0) >= 0;
}

}  // namespace helper
}  // namespace authpolicy
