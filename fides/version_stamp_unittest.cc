// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>

#include "fides/version_stamp.h"

namespace fides {

TEST(VersionStampTest, Irreflexivity) {
  VersionStamp vs;
  vs.Set("A", 1);
  vs.Set("B", 2);
  vs.Set("C", 2);
  EXPECT_FALSE(vs.IsBefore(vs));
}

TEST(VersionStampTest, Before) {
  VersionStamp lhs;
  lhs.Set("A", 1);
  lhs.Set("B", 2);
  lhs.Set("C", 2);

  VersionStamp rhs;
  rhs.Set("A", 1);
  rhs.Set("B", 2);
  rhs.Set("C", 3);

  EXPECT_TRUE(lhs.IsBefore(rhs));
}

TEST(VersionStampTest, BeforeMissingComponentMiddle) {
  VersionStamp lhs;
  lhs.Set("A", 1);
  lhs.Set("B", 2);
  lhs.Set("C", 2);

  VersionStamp rhs;
  rhs.Set("A", 1);
  rhs.Set("C", 2);

  EXPECT_FALSE(lhs.IsBefore(rhs));
  EXPECT_TRUE(rhs.IsBefore(lhs));
}

TEST(VersionStampTest, BeforeMissingComponentLast) {
  VersionStamp lhs;
  lhs.Set("A", 1);
  lhs.Set("B", 2);
  lhs.Set("C", 2);

  VersionStamp rhs;
  rhs.Set("A", 1);
  rhs.Set("B", 2);

  EXPECT_FALSE(lhs.IsBefore(rhs));
  EXPECT_TRUE(rhs.IsBefore(lhs));
  EXPECT_TRUE(lhs.IsAfter(rhs));
}

TEST(VersionStampTest, Concurrent) {
  VersionStamp lhs;
  lhs.Set("A", 1);
  lhs.Set("B", 2);
  lhs.Set("C", 3);

  VersionStamp rhs;
  rhs.Set("A", 1);
  rhs.Set("B", 3);
  rhs.Set("C", 2);

  EXPECT_FALSE(lhs.IsBefore(rhs));
  EXPECT_FALSE(rhs.IsBefore(lhs));
  EXPECT_FALSE(rhs.IsAfter(lhs));
  EXPECT_FALSE(lhs.IsAfter(rhs));
  EXPECT_TRUE(rhs.IsConcurrent(lhs));
  EXPECT_TRUE(lhs.IsConcurrent(rhs));
}

TEST(VersionStampTest, ConcurrentMissingComponentMiddle) {
  VersionStamp lhs;
  lhs.Set("A", 1);
  lhs.Set("B", 2);
  lhs.Set("C", 2);

  VersionStamp rhs;
  rhs.Set("A", 1);
  rhs.Set("C", 3);

  EXPECT_FALSE(lhs.IsBefore(rhs));
  EXPECT_FALSE(rhs.IsBefore(lhs));
}

}  // namespace fides
