// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mount_stack.h"

#include <gtest/gtest.h>

TEST(MountStackTest, Correctness) {
  MountStack stack;
  stack.Push("/foo");
  stack.Push("/bar");
  std::string result;
  EXPECT_TRUE(stack.Pop(&result));
  EXPECT_EQ(result, "/bar");
  EXPECT_TRUE(stack.Pop(&result));
  EXPECT_EQ(result, "/foo");
  EXPECT_FALSE(stack.Pop(&result));
}
