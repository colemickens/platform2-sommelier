// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LOGIN_MANAGER_SYSTEM_UTILS_H_
#define LOGIN_MANAGER_SYSTEM_UTILS_H_

#include <errno.h>
#include <unistd.h>

#include <base/basictypes.h>

namespace login_manager {
class SystemUtils {
 public:
  SystemUtils() {}
  virtual ~SystemUtils() {}

  virtual int kill(pid_t pid, int signal) {
    return ::kill(pid, signal);
  }

  virtual bool child_is_gone(pid_t child_spec) {
    return ::waitpid(child_spec, NULL, WNOHANG) == -1 && errno == ECHILD;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(SystemUtils);
};
}  // namespace login_manager

#endif  // LOGIN_MANAGER_SYSTEM_UTILS_H_
