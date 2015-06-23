// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>
#include <map>
#include <set>
#include <string>

#include "settingsd/identifier_utils.h"

namespace settingsd {

TEST(IdentifierUtilsTest, GetChildPrefixes) {
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

TEST(IdentifierUtilsTest, GetChildPrefixesForRoot) {
  std::map<Key, int> prefix_map = {
      {Key("A.A.B.C"), 0},
      {Key("A.A.B.C.D"), 1}
  };

  auto range = utils::GetRange(Key(), prefix_map);
  EXPECT_EQ(prefix_map.size(), std::distance(range.begin(), range.end()));
  EXPECT_TRUE(std::equal(prefix_map.begin(), prefix_map.end(), range.begin()));
}

}  // namespace settingsd
