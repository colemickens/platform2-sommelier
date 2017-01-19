// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "authpolicy/pipe_helper.h"

#include <cstdint>
#include <fcntl.h>
#include <poll.h>

#include <algorithm>

#include <base/files/file_util.h>

namespace authpolicy {
namespace helper {

namespace {

// Size limit on the total number of bytes to read from a pipe.
const size_t kMaxReadSize = 16 * 1024 * 1024;  // 16 MB
// The size of the buffer used to read from a pipe.
const size_t kBufferSize = PIPE_BUF;  // ~4 Kb on my system
// Timeout used for poll()ing pipes.
const int kPollTimeoutMilliseconds = 30000;

// Reads up to |kBufferSize| bytes from the file descriptor |src_fd| and appends
// them to |dst_str|. Sets |out_done| to true iff the whole file was read
// successfully. Might block if |src_fd| is a blocking pipe. Returns false on
// error.
bool ReadPipe(int src_fd, std::string* dst_str, bool* out_done) {
  *out_done = false;
  char buffer[kBufferSize];
  const ssize_t bytes_read = HANDLE_EINTR(read(src_fd, buffer, kBufferSize));
  if (bytes_read < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
    PLOG(ERROR) << "read() from fd " << src_fd << " failed";
    return false;
  }
  if (bytes_read == 0)
    *out_done = true;
  else if (bytes_read > 0)
    dst_str->append(buffer, bytes_read);
  return true;
}

// Splices (copies) as much as possible data from the file descriptor |src_fd|
// to |dst_fd|. Sets |out_done| to true iff the whole |src_fd| file was spliced
// successfully. Might block if |src_fd| or |dst_fd| are blocking pipes. Returns
// false on error.
bool SplicePipe(int dst_fd, int src_fd, bool* out_done) {
  *out_done = false;
  const int flags = SPLICE_F_NONBLOCK | SPLICE_F_MORE | SPLICE_F_MOVE;
  const ssize_t bytes_written =
      HANDLE_EINTR(splice(src_fd, nullptr, dst_fd, nullptr, INT_MAX, flags));
  if (bytes_written < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
    PLOG(ERROR) << "splice() " << src_fd << " failed";
    return false;
  }
  if (bytes_written == 0)
    *out_done = true;
  return true;
}

// Writes as much as possible from |src_str|, beginning at position |src_pos|,
// to the file descriptor |dst_fd|. Sets |out_done| to true iff the whole string
// was written successfully. Handles blocking |dst_fd|. Returns false on error.
// On success, increases |src_pos| by the number of bytes written.
bool WritePipe(int dst_fd,
               const std::string& src_str,
               size_t* src_pos,
               bool* out_done) {
  DCHECK_LE(*src_pos, src_str.size());
  // Writing 0 bytes might not be well defined, so early out in this case.
  if (*src_pos == src_str.size()) {
    *out_done = true;
    return true;
  }
  *out_done = false;
  const ssize_t bytes_written = HANDLE_EINTR(
      write(dst_fd, src_str.data() + *src_pos, src_str.size() - *src_pos));
  if (bytes_written < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
    PLOG(ERROR) << "write() to " << dst_fd << " failed";
    return false;
  }
  if (bytes_written > 0) {
    *src_pos += bytes_written;
    DCHECK_LE(*src_pos, src_str.size());
  }
  if (*src_pos == src_str.size())
    *out_done = true;
  return true;
}

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

bool PerformPipeIo(int stdin_fd,
                   int stdout_fd,
                   int stderr_fd,
                   int input_fd,
                   const std::string& input_str,
                   std::string* out_stdout,
                   std::string* out_stderr) {
  // Make sure pipes get closed when exiting the scope.
  base::ScopedFD stdin_scoped_fd(stdin_fd);
  base::ScopedFD stdout_scoped_fd(stdout_fd);
  base::ScopedFD stderr_scoped_fd(stderr_fd);

  size_t input_str_pos = 0;
  bool splicing_input_fd = (input_fd != -1);

  while (stdin_scoped_fd.is_valid() || stdout_scoped_fd.is_valid() ||
         stderr_scoped_fd.is_valid()) {
    // Note that closed files (*_scoped_fd.get() == -1) are ignored.
    const int kIndexStdin = 0, kIndexStdout = 1, kIndexStderr = 2;
    const int kPollCount = 3;
    struct pollfd poll_fds[kPollCount];
    poll_fds[kIndexStdin] = {stdin_scoped_fd.get(), POLLOUT, 0};
    poll_fds[kIndexStdout] = {stdout_scoped_fd.get(), POLLIN, 0};
    poll_fds[kIndexStderr] = {stderr_scoped_fd.get(), POLLIN, 0};

    const int poll_result =
        HANDLE_EINTR(poll(poll_fds, kPollCount, kPollTimeoutMilliseconds));
    LOG(INFO) << "Poll result " << poll_result;
    if (poll_result < 0) {
      PLOG(ERROR) << "poll() failed";
      return false;
    }
    for (int n = 0; n < kPollCount; ++n) {
      if (poll_fds[n].revents & (POLLERR | POLLNVAL)) {
        LOG(ERROR) << "POLLERR or POLLNVAL for fd " << poll_fds[n].fd;
        return false;
      }
    }

    // Should only happen on timeout. Log a warning here, so we get at least a
    // log if the process is stale.
    if (poll_result == 0)
      LOG(WARNING) << "poll() timed out. Process might be stale.";

    // Read stdout to the out_stdout string.
    if (poll_fds[kIndexStdout].revents & (POLLIN | POLLHUP)) {
      bool done = false;
      if (!ReadPipe(stdout_scoped_fd.get(), out_stdout, &done))
        return false;
      else if (done)
        stdout_scoped_fd.reset();
    }

    // Read stderr to the out_stderr string.
    if (poll_fds[kIndexStderr].revents & (POLLIN | POLLHUP)) {
      bool done = false;
      if (!ReadPipe(stderr_scoped_fd.get(), out_stderr, &done))
        return false;
      else if (done)
        stderr_scoped_fd.reset();
    }

    if (poll_fds[kIndexStdin].revents & POLLOUT) {
      bool done = false;
      if (splicing_input_fd) {
        // Splice input_fd to stdin_scoped_fd.
        if (!SplicePipe(stdin_scoped_fd.get(), input_fd, &done))
          return false;
        else if (done)
          splicing_input_fd = false;
      } else {
        // Write input_str to stdin_scoped_fd.
        if (!WritePipe(stdin_scoped_fd.get(), input_str, &input_str_pos, &done))
          return false;
        else if (done)
          stdin_scoped_fd.reset();
      }
    }

    // Check size limits.
    if (out_stdout->size() > kMaxReadSize ||
        out_stderr->size() > kMaxReadSize) {
      LOG(ERROR) << "Hit size limit";
      return false;
    }
  }
  return true;
}

}  // namespace helper
}  // namespace authpolicy
