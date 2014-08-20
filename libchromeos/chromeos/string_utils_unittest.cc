// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <chromeos/string_utils.h>

#include <gtest/gtest.h>

namespace chromeos {

TEST(StringUtils, Split) {
  std::vector<std::string> parts;

  parts = string_utils::Split(",a,bc , d,,e,", ',', true, true);
  EXPECT_EQ(4, parts.size());
  EXPECT_EQ("a", parts[0]);
  EXPECT_EQ("bc", parts[1]);
  EXPECT_EQ("d", parts[2]);
  EXPECT_EQ("e", parts[3]);

  parts = string_utils::Split(",a,bc , d,,e,", ',', false, true);
  EXPECT_EQ(4, parts.size());
  EXPECT_EQ("a", parts[0]);
  EXPECT_EQ("bc ", parts[1]);
  EXPECT_EQ(" d", parts[2]);
  EXPECT_EQ("e", parts[3]);

  parts = string_utils::Split(",a,bc , d,,e,", ',', true, false);
  EXPECT_EQ(7, parts.size());
  EXPECT_EQ("", parts[0]);
  EXPECT_EQ("a", parts[1]);
  EXPECT_EQ("bc", parts[2]);
  EXPECT_EQ("d", parts[3]);
  EXPECT_EQ("", parts[4]);
  EXPECT_EQ("e", parts[5]);
  EXPECT_EQ("", parts[6]);

  parts = string_utils::Split(",a,bc , d,,e,", ',', false, false);
  EXPECT_EQ(7, parts.size());
  EXPECT_EQ("", parts[0]);
  EXPECT_EQ("a", parts[1]);
  EXPECT_EQ("bc ", parts[2]);
  EXPECT_EQ(" d", parts[3]);
  EXPECT_EQ("", parts[4]);
  EXPECT_EQ("e", parts[5]);
  EXPECT_EQ("", parts[6]);
}

TEST(StringUtils, SplitAtFirst) {
  std::pair<std::string, std::string> pair;

  pair = string_utils::SplitAtFirst(" 123 : 4 : 56 : 789 ", ':', true);
  EXPECT_EQ("123", pair.first);
  EXPECT_EQ("4 : 56 : 789", pair.second);

  pair = string_utils::SplitAtFirst(" 123 : 4 : 56 : 789 ", ':', false);
  EXPECT_EQ(" 123 ", pair.first);
  EXPECT_EQ(" 4 : 56 : 789 ", pair.second);

  pair = string_utils::SplitAtFirst("", '=');
  EXPECT_EQ("", pair.first);
  EXPECT_EQ("", pair.second);

  pair = string_utils::SplitAtFirst("=", '=');
  EXPECT_EQ("", pair.first);
  EXPECT_EQ("", pair.second);

  pair = string_utils::SplitAtFirst("a=", '=');
  EXPECT_EQ("a", pair.first);
  EXPECT_EQ("", pair.second);

  pair = string_utils::SplitAtFirst("abc=", '=');
  EXPECT_EQ("abc", pair.first);
  EXPECT_EQ("", pair.second);

  pair = string_utils::SplitAtFirst("=a", '=');
  EXPECT_EQ("", pair.first);
  EXPECT_EQ("a", pair.second);

  pair = string_utils::SplitAtFirst("=abc=", '=');
  EXPECT_EQ("", pair.first);
  EXPECT_EQ("abc=", pair.second);

  pair = string_utils::SplitAtFirst("abc", '=');
  EXPECT_EQ("abc", pair.first);
  EXPECT_EQ("", pair.second);
}

TEST(StringUtils, Join_Char) {
  EXPECT_EQ("", string_utils::Join(',', {}));
  EXPECT_EQ("abc", string_utils::Join(',', {"abc"}));
  EXPECT_EQ("abc,defg", string_utils::Join(',', {"abc", "defg"}));
  EXPECT_EQ("1:2:3", string_utils::Join(':', {"1", "2", "3"}));
  EXPECT_EQ("192.168.0.1", string_utils::Join('.', {"192", "168", "0", "1"}));
  EXPECT_EQ("ff02::1", string_utils::Join(':', {"ff02", "", "1"}));
}

TEST(StringUtils, Join_String) {
  EXPECT_EQ("", string_utils::Join(",", {}));
  EXPECT_EQ("abc", string_utils::Join(",", {"abc"}));
  EXPECT_EQ("abc,defg", string_utils::Join(",", {"abc", "defg"}));
  EXPECT_EQ("1 : 2 : 3", string_utils::Join(" : ", {"1", "2", "3"}));
  EXPECT_EQ("123", string_utils::Join("", {"1", "2", "3"}));
}

TEST(StringUtils, Join_Pair) {
  EXPECT_EQ("ab,cd", string_utils::Join(',', "ab", "cd"));
  EXPECT_EQ("key = value", string_utils::Join(" = ", "key", "value"));
}

}  // namespace chromeos
