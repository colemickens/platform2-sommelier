// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef AUTHPOLICY_PLATFORM_HELPER_H_
#define AUTHPOLICY_PLATFORM_HELPER_H_

#include <string>

namespace authpolicy {
namespace helper {

// Reads the whole contents of the file descriptor |fd| into the string |out|.
// If fd is a blocking pipe this call will block until the pipe is closed.
// Returns true iff the whole pipe was successfully read and the pipe was
// smaller than some limit (see code).
extern bool ReadPipeToString(int fd, std::string* out);

// Performs concurrent IO for three different pipes:
// - Reads data from |stdout_fd| into |stdout|.
// - Reads data from |stderr_fd| into |stderr|.
// - Writes data from |input_str| into |stdin_fd|.
//   If |input_fd| is not -1, splices the whole pipe into |stdin_fd| first.
// Returns false on error. May block if any of the pipes is a blocking pipe.
extern bool PerformPipeIo(int stdin_fd,
                          int stdout_fd,
                          int stderr_fd,
                          int input_fd,
                          const std::string& input_str,
                          std::string* stdout,
                          std::string* stderr);

// Gets a user id by name. Dies on error.
uid_t GetUserId(const char* user_name);

// Sets the given uid as saved uid and drops caps. This way, the uid can be
// switched to the saved uid even without keeping caps around. That's more
// secure.
bool SetSavedUserAndDropCaps(uid_t saved_uid);

// Helper class that switches to the saved uid in its scope by swapping the
// real/effective uid with the saved uid. The actual real and effective uid have
// to be equal. Dies on error.
class ScopedUidSwitch {
 public:
  ScopedUidSwitch(uid_t real_and_effective_uid, uid_t saved_uid);
  ~ScopedUidSwitch();

 private:
  uid_t real_and_effective_uid_;
  uid_t saved_uid_;
};

}  // namespace helper
}  // namespace authpolicy

#endif  // AUTHPOLICY_PLATFORM_HELPER_H_
