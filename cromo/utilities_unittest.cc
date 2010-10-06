// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// Utilities for the cromo modem manager

#include "utilities.h"

#include <gflags/gflags.h>
#include <glog/logging.h>
#include <gtest/gtest.h>

TEST(Utilities, ExtractString) {
  using utilities::ExtractString;
  using utilities::DBusPropertyMap;

  DBusPropertyMap m;

  m["string"].writer().append_string("string");
  m["int32"].writer().append_int32(1);

  DBus::Error e_string;
  EXPECT_STREQ("string", ExtractString(m, "string", NULL,  e_string));
  EXPECT_FALSE(e_string.is_set());

  DBus::Error e_int32;
  EXPECT_EQ(NULL, ExtractString(m, "int32", NULL, e_int32));
  EXPECT_TRUE(e_int32.is_set());

  DBus::Error e_missing;
  EXPECT_STREQ("default",
               ExtractString(m, "not present", "default", e_missing));
  EXPECT_FALSE(e_missing.is_set());

  DBus::Error e_repeated_errors;
  EXPECT_STREQ("default",
               ExtractString(m, "int32", "default", e_repeated_errors));
  EXPECT_TRUE(e_repeated_errors.is_set());
  EXPECT_STREQ("default",
               ExtractString(m, "int32", "default", e_repeated_errors));
  EXPECT_TRUE(e_repeated_errors.is_set());
}

TEST(Utilities, HexEsnToDecimal) {
  using utilities::HexEsnToDecimal;

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
