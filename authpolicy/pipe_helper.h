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

// Performs concurrent IO for three different pipes:
// - Reads data from |stdout_fd| into |out_stdout|.
// - Reads data from |stderr_fd| into |out_stderr|.
// - Writes data from |input_str| into |stdin_fd|.
//   If |input_fd| is not -1, splices the whole pipe into |stdin_fd| first.
// Returns false on error. May block if any of the pipes is a blocking pipe.
extern bool PerformPipeIo(int stdin_fd,
                          int stdout_fd,
                          int stderr_fd,
                          int input_fd,
                          const std::string& input_str,
                          std::string* out_stdout,
                          std::string* out_stderr);

}  // namespace helper
}  // namespace authpolicy

#endif  // AUTHPOLICY_PIPE_HELPER_H_
