// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cros-disks/sandboxed-process.h"

#include <gtest/gtest.h>

namespace cros_disks {

class SandboxedProcessTest : public ::testing::Test {
 protected:
  SandboxedProcess process_;
};

TEST_F(SandboxedProcessTest, BuildEmptyArgumentsArray) {
  EXPECT_FALSE(process_.arguments_buffer_.get());
  EXPECT_FALSE(process_.arguments_array_.get());

  process_.BuildArgumentsArray();
  EXPECT_TRUE(process_.arguments_buffer_.get());
  EXPECT_TRUE(process_.arguments_array_.get());
  EXPECT_STREQ(NULL, process_.arguments_array_[0]);
}

TEST_F(SandboxedProcessTest, BuildArgumentsArray) {
  static const char* const kTestArguments[] = {
    "/bin/ls", "-l", "", "."
  };
  static const size_t kNumTestArguments = arraysize(kTestArguments);
  for (size_t i = 0; i < kNumTestArguments; ++i) {
    process_.AddArgument(kTestArguments[i]);
  }

  process_.BuildArgumentsArray();
  EXPECT_TRUE(process_.arguments_buffer_.get());
  EXPECT_TRUE(process_.arguments_array_.get());
  for (size_t i = 0; i < kNumTestArguments; ++i) {
    EXPECT_STREQ(kTestArguments[i], process_.arguments_array_[i]);
  }
  EXPECT_STREQ(NULL, process_.arguments_array_[kNumTestArguments]);
}

}  // namespace cros_disks
