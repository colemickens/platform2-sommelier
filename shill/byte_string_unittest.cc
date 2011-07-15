// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <endian.h>

#include <gtest/gtest.h>

#include "shill/byte_string.h"

using testing::Test;

namespace shill {

namespace {
const unsigned char kTest1[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 };
const unsigned char kTest2[] = { 1, 2, 3, 4 };
const unsigned char kTest3[] = { 0, 0, 0, 0 };
}  // namespace {}

class ByteStringTest : public Test {
};

TEST_F(ByteStringTest, Empty) {
  uint32 val;

  ByteString bs1(0);
  EXPECT_EQ(0, bs1.GetLength());
  EXPECT_TRUE(bs1.GetData() == NULL);
  EXPECT_FALSE(bs1.ConvertToNetUInt32(&val));
  EXPECT_TRUE(bs1.IsZero());
}

TEST_F(ByteStringTest, NonEmpty) {
  ByteString bs1(kTest1, sizeof(kTest1));
  uint32 val;

  EXPECT_EQ(sizeof(kTest1), bs1.GetLength());
  for (unsigned int i = 0; i < sizeof(kTest1); i++) {
    EXPECT_EQ(bs1.GetData()[i], kTest1[i]);
  }
  EXPECT_TRUE(bs1.GetData() != NULL);
  EXPECT_FALSE(bs1.ConvertToNetUInt32(&val));
  EXPECT_FALSE(bs1.IsZero());

  ByteString bs2(kTest2, sizeof(kTest2));
  EXPECT_EQ(sizeof(kTest2), bs2.GetLength());
  for (unsigned int i = 0; i < sizeof(kTest2); i++) {
    EXPECT_EQ(bs2.GetData()[i], kTest2[i]);
  }
  EXPECT_TRUE(bs2.GetData() != NULL);
  EXPECT_FALSE(bs2.IsZero());

  EXPECT_FALSE(bs2.Equals(bs1));

  ByteString bs3(kTest3, sizeof(kTest3));
  EXPECT_EQ(sizeof(kTest3), bs3.GetLength());
  for (unsigned int i = 0; i < sizeof(kTest3); i++) {
    EXPECT_EQ(bs3.GetData()[i], kTest3[i]);
  }
  EXPECT_TRUE(bs3.GetData() != NULL);
  EXPECT_TRUE(bs3.IsZero());
  EXPECT_FALSE(bs2.Equals(bs1));
  EXPECT_FALSE(bs3.Equals(bs1));

  ByteString bs4(kTest1, sizeof(kTest1));
  EXPECT_TRUE(bs4.Equals(bs1));
}

TEST_F(ByteStringTest, UInt32) {
  ByteString bs1 = ByteString::CreateFromNetUInt32(0x1020304);
  uint32 val;

  EXPECT_EQ(4, bs1.GetLength());
  EXPECT_TRUE(bs1.GetData() != NULL);
  EXPECT_TRUE(bs1.ConvertToNetUInt32(&val));
  EXPECT_EQ(0x1020304, val);
  EXPECT_FALSE(bs1.IsZero());

  ByteString bs2(kTest2, sizeof(kTest2));
  EXPECT_TRUE(bs1.Equals(bs2));
  EXPECT_TRUE(bs2.ConvertToNetUInt32(&val));
  EXPECT_EQ(0x1020304, val);

  ByteString bs3 = ByteString::CreateFromCPUUInt32(0x1020304);
  EXPECT_EQ(4, bs1.GetLength());
  EXPECT_TRUE(bs3.GetData() != NULL);
  EXPECT_TRUE(bs3.ConvertToCPUUInt32(&val));
  EXPECT_EQ(0x1020304, val);
  EXPECT_FALSE(bs3.IsZero());

#if __BYTE_ORDER == __LITTLE_ENDIAN
  EXPECT_FALSE(bs1.Equals(bs3));
#else
  EXPECT_TRUE(bs1.Equals(bs3));
#endif
}

}  // namespace shill
