// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cros-disks/process.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace cros_disks {

// A mock process class for testing the process base class.
class ProcessUnderTest : public Process {
 public:
  ProcessUnderTest() {}

  MOCK_METHOD0(Start, bool());
};

class ProcessTest : public ::testing::Test {
 protected:
  ProcessUnderTest process_;
};

TEST_F(ProcessTest, GetArguments) {
  static const char* const kTestArguments[] = {
    "/bin/ls", "-l", "", "."
  };
  static const size_t kNumTestArguments = arraysize(kTestArguments);
  for (size_t i = 0; i < kNumTestArguments; ++i) {
    process_.AddArgument(kTestArguments[i]);
  }

  char** arguments = process_.GetArguments();
  EXPECT_TRUE(arguments != NULL);
  for (size_t i = 0; i < kNumTestArguments; ++i) {
    EXPECT_STREQ(kTestArguments[i], arguments[i]);
  }
  EXPECT_STREQ(NULL, arguments[kNumTestArguments]);
}

TEST_F(ProcessTest, GetArgumentsWithNoArgumentsAdded) {
  EXPECT_TRUE(process_.GetArguments() == NULL);
}

}  // namespace cros_disks
