// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DIAGNOSTICS_DIAGNOSTICSD_FILE_TEST_UTILS_H_
#define DIAGNOSTICS_DIAGNOSTICSD_FILE_TEST_UTILS_H_

#include <string>

#include <base/files/file.h>

namespace diagnostics {

// Write |file_contents| into file located in |file_path|. Also will create all
// nested parent directories if necessary.
bool WriteFileAndCreateParentDirs(const base::FilePath& file_path,
                                  const std::string& file_contents);

}  // namespace diagnostics

#endif  // DIAGNOSTICS_DIAGNOSTICSD_FILE_TEST_UTILS_H_
