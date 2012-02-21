// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "login_manager/file_checker.h"

#include <base/file_path.h>
#include <base/file_util.h>

namespace login_manager {

FileChecker::FileChecker(const FilePath& filename) : filename_(filename) {}

FileChecker::~FileChecker() {}

bool FileChecker::exists() {
  return file_util::PathExists(filename_);
}

}  // namespace login_manager
