// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <bluetooth/newblued/util.h>

#include <vector>

#include <gtest/gtest.h>

namespace bluetooth {

TEST(UtilTest, GetFromLE) {
  uint8_t le16[] = {0x11, 0x22};
  uint8_t le24[] = {0x33, 0x44, 0x55};
  uint8_t le_bytes[] = {0x11, 0x22, 0x33, 0x44, 0x55};
  std::vector<uint8_t> expected_bytes({0x55, 0x44, 0x33, 0x22, 0x11});

  EXPECT_EQ(0x2211, GetNumFromLE16(le16));
  EXPECT_EQ(0x554433, GetNumFromLE24(le24));
  EXPECT_EQ(expected_bytes, GetBytesFromLE(le_bytes, sizeof(le_bytes)));

  EXPECT_TRUE(GetBytesFromLE(le_bytes, 0).empty());
}

TEST(UtilTest, GetNextId) {
  UniqueId id1 = GetNextId();
  UniqueId id2 = GetNextId();

  EXPECT_NE(kInvalidUniqueId, id1);
  EXPECT_NE(kInvalidUniqueId, id2);
  EXPECT_NE(id1, id2);
  EXPECT_LT(id1, id2);
}

}  // namespace bluetooth
