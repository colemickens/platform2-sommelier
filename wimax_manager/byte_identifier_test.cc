// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "wimax_manager/byte_identifier.h"

#include <gtest/gtest.h>

namespace wimax_manager {

class ByteIdentifierTest : public testing::Test {};

TEST_F(ByteIdentifierTest, ConstructorWithOnlyLength) {
  uint8_t expected[6] = {0x00};
  ByteIdentifier identifier(arraysize(expected));
  EXPECT_EQ(arraysize(expected), identifier.data().size());
  EXPECT_EQ(0, memcmp(&identifier.data()[0], expected, sizeof(expected)));
}

TEST_F(ByteIdentifierTest, GetData) {
  uint8_t test_data[] = {0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef};
  ByteIdentifier identifier(test_data, arraysize(test_data));
  EXPECT_EQ(arraysize(test_data), identifier.data().size());
  EXPECT_EQ(0, memcmp(&identifier.data()[0], test_data, sizeof(test_data)));
}

TEST_F(ByteIdentifierTest, GetHexString) {
  uint8_t test_data[] = {0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef};
  ByteIdentifier identifier(test_data, arraysize(test_data));
  EXPECT_EQ("01:23:45:67:89:ab:cd:ef", identifier.GetHexString());
}

TEST_F(ByteIdentifierTest, CopyFrom) {
  uint8_t test_data1[] = {0x00};
  uint8_t test_data2[] = {0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef};
  ByteIdentifier identifier1(test_data1, arraysize(test_data1));
  ByteIdentifier identifier2(test_data2, arraysize(test_data2));

  EXPECT_EQ(arraysize(test_data1), identifier1.data().size());
  EXPECT_EQ(0, memcmp(&identifier1.data()[0], test_data1, sizeof(test_data1)));

  identifier1.CopyFrom(identifier2);
  EXPECT_EQ(arraysize(test_data2), identifier1.data().size());
  EXPECT_EQ(0, memcmp(&identifier1.data()[0], test_data2, sizeof(test_data2)));
}

}  // namespace wimax_manager
