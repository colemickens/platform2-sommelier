// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromiumos-wide-profiling/scoped_temp_path.h"

#include <sys/stat.h>

#include <vector>

#include "chromiumos-wide-profiling/quipper_test.h"

namespace {

// For testing the creation of multiple temp paths.
const int kNumMultiplePaths = 32;

// Tests if |path| exists on the file system.
bool PathExists(const string& path) {
  struct stat buf;
  // stat() returns 0 on success, i.e. if the path exists and is valid.
  return !stat(path.c_str(), &buf);
}

}  // namespace

namespace quipper {

// Create one file and make sure it is deleted when out of scope.
TEST(ScopedTempPathTest, OneFile) {
  string path;
  {
    ScopedTempFile temp_file;
    path = temp_file.path();
    EXPECT_TRUE(PathExists(path)) << path;
  }
  EXPECT_FALSE(PathExists(path)) << path;
}

// Create many files and make sure they are deleted when out of scope.
TEST(ScopedTempPathTest, MultipleFiles) {
  std::vector<string> paths(kNumMultiplePaths);
  {
    std::vector<ScopedTempFile> temp_files(kNumMultiplePaths);
    for (size_t i = 0; i < kNumMultiplePaths; ++i) {
      paths[i] = temp_files[i].path();
      EXPECT_TRUE(PathExists(paths[i])) << paths[i];
    }
  }
  for (size_t i = 0; i < kNumMultiplePaths; ++i) {
    EXPECT_FALSE(PathExists(paths[i])) << paths[i];
  }
}

// Create one empty directory and make sure it is deleted when out of scope.
TEST(ScopedTempPathTest, OneEmptyDir) {
  string path;
  {
    ScopedTempDir temp_path;
    path = temp_path.path();
    EXPECT_TRUE(PathExists(path)) << path;
  }
  EXPECT_FALSE(PathExists(path)) << path;
}

// Create many empty directories and make sure they are deleted when out of
// scope.
TEST(ScopedTempPathTest, MultipleEmptyDirs) {
  std::vector<string> paths(kNumMultiplePaths);
  {
    std::vector<ScopedTempDir> temp_dirs(kNumMultiplePaths);
    for (size_t i = 0; i < kNumMultiplePaths; ++i) {
      paths[i] = temp_dirs[i].path();
      EXPECT_TRUE(PathExists(paths[i])) << paths[i];
    }
  }
  for (size_t i = 0; i < kNumMultiplePaths; ++i) {
    EXPECT_FALSE(PathExists(paths[i])) << paths[i];
  }
}

// TODO(sque): Add tests for non-empty directories.

}  // namespace quipper

int main(int argc, char * argv[]) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
