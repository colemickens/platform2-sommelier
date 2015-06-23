// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "settingsd/key.h"

#include <gtest/gtest.h>

namespace settingsd {

TEST(KeyTest, GetParent) {
  Key A("A.B");
  Key parent = A.GetParent();
  EXPECT_EQ(parent, Key("A"));
}

TEST(KeyTest, GetParentForRoot) {
  EXPECT_EQ(Key().GetParent(), Key());
}

}  // namespace settingsd
