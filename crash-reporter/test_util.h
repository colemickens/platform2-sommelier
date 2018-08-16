// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRASH_REPORTER_TEST_UTIL_H_
#define CRASH_REPORTER_TEST_UTIL_H_

#include <base/files/file_path.h>
#include <base/strings/string_piece.h>

namespace test_util {

// Creates a file at |file_path| with |content|, with parent directories.
// Returns true on success. If you want the test function to stop when the file
// creation failed, wrap this function with ASSERT_TRUE().
bool CreateFile(const base::FilePath& file_path, base::StringPiece content);

}  // namespace test_util

#endif  // CRASH_REPORTER_TEST_UTIL_H_
