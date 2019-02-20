// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DIAGNOSTICS_WILCO_DTC_SUPPORTD_FILE_TEST_UTILS_H_
#define DIAGNOSTICS_WILCO_DTC_SUPPORTD_FILE_TEST_UTILS_H_

#include <string>

#include <base/files/file.h>

namespace diagnostics {

// Write |file_contents| into file located in |file_path|. Also will create all
// nested parent directories if necessary.
bool WriteFileAndCreateParentDirs(const base::FilePath& file_path,
                                  const std::string& file_contents);

// Create the directory located in |file_path|, and create a symbolic link under
// |file_path| pointing back to |file_path|. Will create all nested parent
// directories if necessary.
bool CreateCyclicSymbolicLink(const base::FilePath& file_path);

// Write |file_contents| into file located in |file_path|, then create a
// symbolic link which points to |file_path|. Will create all nested parent
// directories if necessary.
bool WriteFileAndCreateSymbolicLink(const base::FilePath& file_path,
                                    const std::string& file_contents,
                                    const base::FilePath& symlink_path);

}  // namespace diagnostics

#endif  // DIAGNOSTICS_WILCO_DTC_SUPPORTD_FILE_TEST_UTILS_H_
