// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>
#include <map>
#include <set>
#include <string>

#include "settingsd/identifier_utils.h"

namespace settingsd {

TEST(IdentifierUtilsTest, IsKey) {
  EXPECT_TRUE(utils::IsKey("A"));
  EXPECT_TRUE(utils::IsKey("A.B"));
  EXPECT_FALSE(utils::IsKey("A."));
  EXPECT_FALSE(utils::IsKey("A.B."));
  EXPECT_FALSE(utils::IsKey(""));
}

TEST(IdentifierUtilsTest, GetParentPrefix) {
  EXPECT_EQ("A.B.", utils::GetParentPrefix("A.B.C"));
  EXPECT_EQ("A.", utils::GetParentPrefix("A.B."));
  EXPECT_EQ("", utils::GetParentPrefix("A."));
  EXPECT_EQ("", utils::GetParentPrefix("A"));
  EXPECT_EQ("", utils::GetParentPrefix(""));
}

TEST(IdentifierUtilsTest, GetChildPrefixes) {
  std::map<std::string, int> prefix_map;
  prefix_map["A.A.B.C"] = 0;
  prefix_map["A.A.B.C.D"] = 1;
  prefix_map["A.B"] = 2;
  prefix_map["A.B."] = 3;
  prefix_map["A.B.C"] = 4;
  prefix_map["A.B.C."] = 5;
  prefix_map["A.B.C.D"] = 6;
  prefix_map["A.C.A.B."] = 7;
  prefix_map["A.C.A.B.C"] = 8;

  std::map<std::string, int> expected;
  expected["A.B.C"] = 4;
  expected["A.B.C."] = 5;
  expected["A.B.C.D"] = 6;

  auto range = utils::GetChildPrefixes("A.B.", prefix_map);
  EXPECT_EQ(expected.size(), std::distance(range.begin(), range.end()));
  EXPECT_TRUE(std::equal(expected.begin(), expected.end(), range.begin()));
}

TEST(IdentifierUtilsTest, GetChildPrefixesForRoot) {
  std::map<std::string, int> prefix_map;
  prefix_map["A.A.B.C"] = 0;
  prefix_map["A.A.B.C.D"] = 1;

  auto range = utils::GetChildPrefixes("", prefix_map);
  EXPECT_EQ(prefix_map.size(), std::distance(range.begin(), range.end()));
  EXPECT_TRUE(std::equal(prefix_map.begin(), prefix_map.end(), range.begin()));
}

}  // namespace settingsd
