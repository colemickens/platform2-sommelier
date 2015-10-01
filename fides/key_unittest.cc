// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fides/key.h"

#include <gtest/gtest.h>

#include "fides/test_helpers.h"

namespace fides {

TEST(KeyTest, IsValidKey) {
  EXPECT_TRUE(Key::IsValidKey(""));
  EXPECT_FALSE(Key::IsValidKey("."));
  EXPECT_TRUE(Key::IsValidKey("A"));
  EXPECT_FALSE(Key::IsValidKey(".A"));
  EXPECT_FALSE(Key::IsValidKey("A."));
  EXPECT_TRUE(Key::IsValidKey("A.B"));
  EXPECT_FALSE(Key::IsValidKey("A..B"));
  EXPECT_FALSE(Key::IsValidKey("A.!.B"));
}

TEST(KeyTest, GetParent) {
  EXPECT_EQ(Key().GetParent(), Key());
  EXPECT_EQ(Key("A"), Key("A.B").GetParent());
}

TEST(KeyTest, Append) {
  EXPECT_EQ(Key("A"), Key().Append(Key("A")));
  EXPECT_EQ(Key("A.B"), Key("A").Append(Key("B")));
}

TEST(KeyTest, Extend) {
  EXPECT_EQ(Key("A"), Key().Extend({"A"}));
  EXPECT_EQ(Key("A.B"), Key("A").Extend({"B"}));
  EXPECT_EQ(Key("A.B.C"), Key("A").Extend({"B", "C"}));
}

TEST(KeyTest, Split) {
  Key suffix;
  EXPECT_EQ(Key(), Key().Split(nullptr));
  EXPECT_EQ(Key(), Key().Split(&suffix));
  EXPECT_EQ(Key(), suffix);
  EXPECT_EQ(Key("A"), Key("A").Split(nullptr));
  EXPECT_EQ(Key("A"), Key("A").Split(&suffix));
  EXPECT_EQ(Key(), suffix);
  EXPECT_EQ(Key("A"), Key("A.B").Split(nullptr));
  EXPECT_EQ(Key("A"), Key("A.B").Split(&suffix));
  EXPECT_EQ(Key("B"), suffix);
  EXPECT_EQ(Key("A"), Key("A.B.C").Split(nullptr));
  EXPECT_EQ(Key("A"), Key("A.B.C").Split(&suffix));
  EXPECT_EQ(Key("B.C"), suffix);
}

TEST(KeyTest, CommonPrefix) {
  EXPECT_EQ(Key(), Key().CommonPrefix(Key()));
  EXPECT_EQ(Key(), Key("A").CommonPrefix(Key()));
  EXPECT_EQ(Key(), Key().CommonPrefix(Key("A")));
  EXPECT_EQ(Key("A"), Key("A").CommonPrefix(Key("A")));
  EXPECT_EQ(Key("A"), Key("A.B").CommonPrefix(Key("A")));
  EXPECT_EQ(Key("A"), Key("A").CommonPrefix(Key("A.B")));
  EXPECT_EQ(Key("A"), Key("A.BA.C").CommonPrefix(Key("A.B.C")));
  EXPECT_EQ(Key(), Key("A.B").CommonPrefix(Key("B")));
}

TEST(KeyTest, Suffix) {
  Key suffix;
  EXPECT_TRUE(Key().Suffix(Key(), &suffix));
  EXPECT_EQ(Key(), suffix);
  EXPECT_FALSE(Key().Suffix(Key("A"), &suffix));
  EXPECT_EQ(Key(), suffix);
  EXPECT_TRUE(Key("A").Suffix(Key(""), &suffix));
  EXPECT_EQ(Key("A"), suffix);
  EXPECT_TRUE(Key("A").Suffix(Key("A"), &suffix));
  EXPECT_EQ(Key(""), suffix);
  EXPECT_TRUE(Key("A.B").Suffix(Key("A"), &suffix));
  EXPECT_EQ(Key("B"), suffix);
}

TEST(KeyTest, IsPrefixOf) {
  EXPECT_TRUE(Key().IsPrefixOf(Key()));
  EXPECT_TRUE(Key().IsPrefixOf(Key("A")));
  EXPECT_TRUE(Key("A").IsPrefixOf(Key("A")));
  EXPECT_FALSE(Key("A").IsPrefixOf(Key()));
  EXPECT_TRUE(Key("A.B").IsPrefixOf(Key("A.B.C")));
  EXPECT_FALSE(Key("A.C").IsPrefixOf(Key("A.B.C")));
  EXPECT_FALSE(Key("A.B").IsPrefixOf(Key("A.BC")));
}

}  // namespace fides
