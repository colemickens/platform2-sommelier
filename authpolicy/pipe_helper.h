// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef AUTHPOLICY_PIPE_HELPER_H_
#define AUTHPOLICY_PIPE_HELPER_H_

#include <string>

namespace authpolicy {
namespace helper {

// Reads the whole contents of the file descriptor |fd| into the string |out|.
// If fd is a blocking pipe this call will block until the pipe is closed.
// Returns true iff the whole pipe was successfully read and the pipe was
// smaller than some limit (see code).
extern bool ReadPipeToString(int fd, std::string* out);

// Writes the whole string |str| to the file descriptor |fd|. Does not close
// |fd| when done.  Returns true iff the whole string has been written to |fd|.
// Might block if the underlying pipe is full.
extern bool WriteStringToPipe(const std::string& str, int fd);

// Uses tee() to copy |fd_in| to |fd_out|. Might block if the underlying pipes
// block.
extern bool CopyPipe(int fd_in, int fd_out);

}  // namespace helper
}  // namespace authpolicy

#endif  // AUTHPOLICY_PIPE_HELPER_H_
