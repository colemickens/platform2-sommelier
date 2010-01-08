// Copyright (c) 2009-2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LOGIN_MANAGER_FILE_CHECKER_H_
#define LOGIN_MANAGER_FILE_CHECKER_H_

#include <sys/stat.h>

namespace login_manager {
class FileChecker {
 public:
  FileChecker() {}
  ~FileChecker() {}

  virtual bool exists(const char* filename) {
    struct stat buf;
    return 0 == stat(filename, &buf);
  }
};

}  // namespace login_manager

#endif  // LOGIN_MANAGER_FILE_CHECKER_H_
