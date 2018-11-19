// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "diagnostics/diagnosticsd/file_test_utils.h"

#include <base/files/file_util.h>

namespace diagnostics {

bool WriteFileAndCreateParentDirs(const base::FilePath& file_path,
                                  const std::string& file_contents) {
  if (!base::CreateDirectory(file_path.DirName())) {
    return false;
  }
  return base::WriteFile(file_path, file_contents.c_str(),
                         file_contents.size()) == file_contents.size();
}

}  // namespace diagnostics
