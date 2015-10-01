// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fides/identifier_utils.h"

#include <gtest/gtest.h>
#include <map>
#include <set>
#include <string>

namespace fides {

TEST(IdentifierUtilsTest, GetRange) {
  std::map<Key, int> prefix_map = {
      {Key("A.A.B.C"), 0},
      {Key("A.A.B.C.D"), 1},
      {Key("A.B"), 2},
      {Key("A.B.C"), 3},
      {Key("A.B.C.D"), 4},
      {Key("A.C.A.B.C"), 5}
  };
  std::map<Key, int> expected = {
      {Key("A.B"), 2},
      {Key("A.B.C"), 3},
      {Key("A.B.C.D"), 4}
  };

  auto range = utils::GetRange(Key("A.B"), prefix_map);
  EXPECT_EQ(expected.size(), std::distance(range.begin(), range.end()));
  EXPECT_TRUE(std::equal(expected.begin(), expected.end(), range.begin()));
}

TEST(IdentifierUtilsTest, GetRangeForRoot) {
  std::map<Key, int> prefix_map = {
      {Key("A.A.B.C"), 0},
      {Key("A.A.B.C.D"), 1}
  };

  auto range = utils::GetRange(Key(), prefix_map);
  EXPECT_EQ(prefix_map.size(), std::distance(range.begin(), range.end()));
  EXPECT_TRUE(std::equal(prefix_map.begin(), prefix_map.end(), range.begin()));
}

TEST(IdentifierUtilsTest, HasKeys) {
  std::set<Key> container = {Key("A.B")};

  EXPECT_TRUE(utils::HasKeys(Key("A"), container));
  EXPECT_FALSE(utils::HasKeys(Key("A.A"), container));
  EXPECT_TRUE(utils::HasKeys(Key("A.B"), container));
  EXPECT_FALSE(utils::HasKeys(Key("A.B.C"), container));
  EXPECT_TRUE(utils::HasKeys(Key(), container));
}

TEST(IdentifierUtilsTest, HasKeysEmptyContainer) {
  std::set<Key> container;
  EXPECT_FALSE(utils::HasKeys(Key(), container));
  EXPECT_FALSE(utils::HasKeys(Key("A"), container));
}

}  // namespace fides
