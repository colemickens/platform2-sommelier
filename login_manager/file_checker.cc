// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "login_manager/file_checker.h"

#include <base/file_path.h>
#include <base/file_util.h>

namespace login_manager {
// static
const char FileChecker::kLegacy[] = "/tmp/disable_chrome_restart";

FileChecker::FileChecker(const FilePath& filename) : filename_(filename) {}

FileChecker::~FileChecker() {}

bool FileChecker::exists() {
  // TODO(cmasone): Remove kLegacy to complete http://crosbug.com/17156
  return file_util::PathExists(filename_) ||
      file_util::PathExists(FilePath(kLegacy));
}

}  // namespace login_manager
