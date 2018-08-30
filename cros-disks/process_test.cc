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
  ProcessUnderTest() = default;

  MOCK_METHOD0(Start, bool());
};

class ProcessTest : public ::testing::Test {
 protected:
  ProcessUnderTest process_;
};

TEST_F(ProcessTest, GetArguments) {
  static const char* const kTestArguments[] = {"/bin/ls", "-l", "", "."};
  for (const char* test_argument : kTestArguments) {
    process_.AddArgument(test_argument);
  }

  char** arguments = process_.GetArguments();
  EXPECT_NE(nullptr, arguments);
  for (const char* test_argument : kTestArguments) {
    EXPECT_STREQ(test_argument, *arguments);
    ++arguments;
  }
  EXPECT_EQ(nullptr, *arguments);
}

TEST_F(ProcessTest, GetArgumentsWithNoArgumentsAdded) {
  EXPECT_EQ(nullptr, process_.GetArguments());
}

}  // namespace cros_disks
