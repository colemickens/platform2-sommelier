// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cellular.h"

#include <gflags/gflags.h>
#include <glog/logging.h>
#include <gtest/gtest.h>

TEST(Cellular, HexEsnToDecimal) {
  using cellular::HexEsnToDecimal;

  std::string out;
  EXPECT_TRUE(HexEsnToDecimal("ffffffff", &out));
  EXPECT_STREQ("25516777215", out.c_str());

  EXPECT_TRUE(HexEsnToDecimal("80abcdef", &out));
  EXPECT_STREQ("12811259375", out.c_str());

  EXPECT_TRUE(HexEsnToDecimal("80000001", &out));
  EXPECT_STREQ("12800000001", out.c_str());

  EXPECT_TRUE(HexEsnToDecimal("1", &out));
  EXPECT_STREQ("00000000001", out.c_str());

  EXPECT_FALSE(HexEsnToDecimal("000bogus", &out));
  EXPECT_FALSE(HexEsnToDecimal("ffffffff" "f", &out));
}

int main(int argc, char *argv[]) {
  testing::InitGoogleTest(&argc, argv);
  google::InitGoogleLogging(argv[0]);
  google::ParseCommandLineFlags(&argc, &argv, true);

  return RUN_ALL_TESTS();
}
