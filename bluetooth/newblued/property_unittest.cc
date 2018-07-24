// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <base/bind.h>
#include <gtest/gtest.h>

#include "bluetooth/newblued/property.h"

namespace bluetooth {

TEST(PropertyTest, SetValueAndEmitChange) {
  Property<int> property(50);

  EXPECT_FALSE(property.updated());
  EXPECT_EQ(50, property.value());

  property.SetValue(20);
  EXPECT_EQ(20, property.value());
  EXPECT_TRUE(property.updated());

  property.ClearUpdated();
  EXPECT_EQ(20, property.value());
  EXPECT_FALSE(property.updated());
}

}  // namespace bluetooth
