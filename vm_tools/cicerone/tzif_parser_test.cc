// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdlib.h>

#include <base/files/file_path.h>
#include <gtest/gtest.h>

#include "vm_tools/cicerone/tzif_parser.h"

class TzifParserTest : public ::testing::Test {
 public:
  TzifParserTest() {
    base::FilePath source_dir(getenv("SRC"));
    tp_.SetZoneinfoDirectoryForTest(source_dir.Append("cicerone"));
  }

 protected:
  TzifParser tp_;
};

TEST_F(TzifParserTest, EST) {
  std::string posix_string;
  EXPECT_TRUE(tp_.GetPosixTimezone("EST_test.tzif", &posix_string));
  EXPECT_EQ(posix_string, "EST5");
}

TEST_F(TzifParserTest, TzifVersionTwo) {
  std::string posix_string;
  EXPECT_TRUE(
      tp_.GetPosixTimezone("Indian_Christmas_test.tzif", &posix_string));
  EXPECT_EQ(posix_string, "<+07>-7");
}

TEST_F(TzifParserTest, TzifVersionThree) {
  std::string posix_string;
  EXPECT_TRUE(tp_.GetPosixTimezone("Pacific_Fiji_test.tzif", &posix_string));
  EXPECT_EQ(posix_string, "<+12>-12<+13>,M11.1.0,M1.2.2/123");
}
