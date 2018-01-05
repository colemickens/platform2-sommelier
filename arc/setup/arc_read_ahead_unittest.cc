// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// How to build and run the tests: see arc_setup_util_unittest.cc

#include "arc/setup/arc_read_ahead.h"

#include <string>

#include <base/files/file_path.h>
#include <base/files/scoped_temp_dir.h>
#include <base/time/time.h>
#include <gtest/gtest.h>

#include "arc/setup/arc_setup_util.h"

namespace arc {
namespace {

// Tests EmulateArcUreadahead() function.
TEST(ArcReadAhead, TestEmulateArcUreadahead) {
  base::ScopedTempDir temp_directory;
  ASSERT_TRUE(temp_directory.CreateUniqueTempDir());
  ASSERT_TRUE(MkdirRecursively(temp_directory.path().Append("subdir")));

  // These files should be read-ahead. Note that both framework-res.apk files
  // must be read.
  EXPECT_TRUE(WriteToFile(temp_directory.path().Append("framework-res.apk"),
                          0755, std::string(0x1, 'x')));
  EXPECT_TRUE(WriteToFile(
      temp_directory.path().Append("subdir").Append("framework-res.apk"), 0755,
      std::string(0x1 << 1, 'x')));
  EXPECT_TRUE(
      WriteToFile(temp_directory.path().Append("PrebuiltGmsCoreRelease.apk"),
                  0755, std::string(0x1 << 2, 'x')));
  EXPECT_TRUE(
      WriteToFile(temp_directory.path().Append("lib_read_ahead_unittest_1.so"),
                  0755, std::string(0x1 << 3, 'x')));
  EXPECT_TRUE(
      WriteToFile(temp_directory.path().Append("read_ahead_unittest_1.ttf"),
                  0755, std::string(0x1 << 4, 'x')));

  // All files below should be ignored.
  EXPECT_TRUE(
      WriteToFile(temp_directory.path().Append("read_ahead_unittest_2.ttf_"),
                  0755, std::string(0x1 << 5, 'x')));
  EXPECT_TRUE(
      WriteToFile(temp_directory.path().Append("read_ahead_unittest_3.ttc"),
                  0755, std::string(0x1 << 6, 'x')));
  // This is a .ttf file, but is empty.
  EXPECT_TRUE(CreateOrTruncate(
      temp_directory.path().Append("read_ahead_unittest_4.ttf"), 0755));

  auto result = EmulateArcUreadahead(temp_directory.path(),
                                     base::TimeDelta::FromSeconds(5));
  EXPECT_EQ(5, result.first);
  EXPECT_EQ(31 /* == 0b11111 */, result.second);
}

}  // namespace
}  // namespace arc
