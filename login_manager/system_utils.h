// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LOGIN_MANAGER_SYSTEM_UTILS_H_
#define LOGIN_MANAGER_SYSTEM_UTILS_H_

#include <errno.h>
#include <unistd.h>

#include <base/basictypes.h>
#include <base/logging.h>

namespace login_manager {
class SystemUtils {
 public:
  SystemUtils() {}
  virtual ~SystemUtils() {}

  virtual int kill(pid_t pid, int signal) {
    LOG(INFO) << "Sending " << signal << " to " << pid;
    return ::kill(pid, signal);
  }

  // Returns: true if child specified by |child_spec| exited,
  //          false if we time out.
  virtual bool child_is_gone(pid_t child_spec, int timeout) {
    alarm(timeout);
    while (::waitpid(child_spec, NULL, 0) > 0)
      ;
    // Once we exit the loop, we know there was an error.
    alarm(0);
    return errno == ECHILD;  // EINTR means we timed out.
  }
 private:
  DISALLOW_COPY_AND_ASSIGN(SystemUtils);
};
}  // namespace login_manager

#endif  // LOGIN_MANAGER_SYSTEM_UTILS_H_
