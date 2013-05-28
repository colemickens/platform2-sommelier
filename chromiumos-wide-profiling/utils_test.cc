// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <gtest/gtest.h>

#include "perf_reader.h"
#include "perf_serializer.h"
#include "perf_protobuf_io.h"
#include "utils.h"

using quipper::Md5Prefix;
using quipper::AlignSize;

TEST(UtilsTest, TestMD5) {
  ASSERT_EQ(Md5Prefix(""), 0xd41d8cd98f00b204LL);
  ASSERT_EQ(Md5Prefix("The quick brown fox jumps over the lazy dog."),
            0xe4d909c290d0fb1cLL);
}

TEST(UtilsTest, TestAlignSize) {
  EXPECT_EQ(12, AlignSize(10, 4));
  EXPECT_EQ(12, AlignSize(12, 4));
  EXPECT_EQ(16, AlignSize(13, 4));
  EXPECT_EQ(100, AlignSize(97, 4));
  EXPECT_EQ(100, AlignSize(100, 4));
  EXPECT_EQ(104, AlignSize(100, 8));
  EXPECT_EQ(112, AlignSize(108, 8));
  EXPECT_EQ(112, AlignSize(112, 8));
}

int main(int argc, char * argv[])
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
