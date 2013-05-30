// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef P2P_SERVER_UTIL_H__
#define P2P_SERVER_UTIL_H__

#include <string>
#include <vector>

#include <base/file_path.h>
#include <gtest/gtest.h>

namespace p2p {

namespace testutil {

// Utility function to to run the command expressed by the
// printf()-style string |format| using the system(3) utility
// function. Will assert unless the command exits normally
// with exit status |expected_exit_status|.
void ExpectCommand(int expected_exit_status, const char* format, ...)
    __attribute__((__format__(__printf__, 2, 3)));

// Creates a unique and empty directory and returns the
// path. Your code should call TeardownTestDir() when
// you are done with it.
base::FilePath SetupTestDir(const std::string& test_name);

// Deletes all files and sub-directories of the directory given by
// |dir_path|. This should only be called on directories
// previously created by SetupTestDir().
void TeardownTestDir(const base::FilePath& dir_path);

// Runs the default GLib main loop for |timeout_msec|.
void RunGMainLoop(int timeout_msec);

// Utility function to determine if two vectors are equal.
template <class T>
bool VectorsEqual(const std::vector<T>& a, const std::vector<T>& b) {
  return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin());
}

// Utility function that asserts unless the given vectors |a| and |b|
// are equal.
template <class T>
void ExpectVectorsEqual(const std::vector<T>& a, const std::vector<T>& b) {
  EXPECT_TRUE(VectorsEqual(a, b));
}

// Utility function to get the size of the file given by |file_name| in
// the directory given by |dir|. If the file does not exist, 0 is
// returned.
size_t FileSize(const base::FilePath& dir, const std::string& file_name);

// Asserts unless the file given by |file_name| in |dir| does not have
// the size given by |expected_size|.
void ExpectFileSize(const base::FilePath& dir,
                    const std::string& file_name,
                    size_t expected_size);

}  // namespace testutil

}  // namespace p2p

#endif  // P2P_SERVER_UTIL_H__
