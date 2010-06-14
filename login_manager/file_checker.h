// Copyright (c) 2009-2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LOGIN_MANAGER_FILE_CHECKER_H_
#define LOGIN_MANAGER_FILE_CHECKER_H_

#include <sys/stat.h>

namespace login_manager {
class FileChecker {
 public:
  explicit FileChecker(std::string filename) : filename_(filename) {}
  virtual ~FileChecker() {}

  virtual bool exists() {
    struct stat buf;
    return 0 == stat(filename_.c_str(), &buf);
  }
 private:
  const std::string filename_;
};

}  // namespace login_manager

#endif  // LOGIN_MANAGER_FILE_CHECKER_H_
