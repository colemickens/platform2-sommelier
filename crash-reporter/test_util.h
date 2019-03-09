// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRASH_REPORTER_TEST_UTIL_H_
#define CRASH_REPORTER_TEST_UTIL_H_

#include <map>
#include <string>

#include <base/files/file_path.h>
#include <base/strings/string_piece.h>

#include <session_manager/dbus-proxy-mocks.h>

namespace test_util {

// Creates a file at |file_path| with |content|, with parent directories.
// Returns true on success. If you want the test function to stop when the file
// creation failed, wrap this function with ASSERT_TRUE().
bool CreateFile(const base::FilePath& file_path, base::StringPiece content);

// Configures |mock| so that RetrieveActiveSessions() returns |sessions|.
void SetActiveSessions(org::chromium::SessionManagerInterfaceProxyMock* mock,
                       const std::map<std::string, std::string>& sessions);

// Returns true if at least one file in this directory matches the pattern.
// found_file_path is not assigned if found_file_path is nullptr.
// Only the first found path is stored into found_file_path.
bool DirectoryHasFileWithPattern(const base::FilePath& directory,
                                 const std::string& pattern,
                                 base::FilePath* found_file_path);

}  // namespace test_util

#endif  // CRASH_REPORTER_TEST_UTIL_H_
