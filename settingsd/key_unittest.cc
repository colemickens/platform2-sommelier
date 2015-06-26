// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "settingsd/key.h"

#include <gtest/gtest.h>

namespace settingsd {

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

}  // namespace settingsd
