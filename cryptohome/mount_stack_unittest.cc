// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cryptohome/mount_stack.h"

#include <base/files/file_path.h>
#include <gtest/gtest.h>

using base::FilePath;

TEST(MountStackTest, Correctness) {
  MountStack stack;
  stack.Push(FilePath("/foo"));
  stack.Push(FilePath("/bar"));
  FilePath result;
  EXPECT_TRUE(stack.Pop(&result));
  EXPECT_EQ(result, FilePath("/bar"));
  EXPECT_TRUE(stack.Pop(&result));
  EXPECT_EQ(result, FilePath("/foo"));
  EXPECT_FALSE(stack.Pop(&result));
}
