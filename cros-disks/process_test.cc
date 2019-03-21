// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cros-disks/process.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace cros_disks {

using testing::ElementsAre;
using testing::Return;

// A mock process class for testing the process base class.
class ProcessUnderTest : public Process {
 public:
  ProcessUnderTest() = default;

  MOCK_METHOD0(Start, bool());
  MOCK_METHOD0(Wait, int());
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

  EXPECT_THAT(process_.arguments(),
              testing::ElementsAre("/bin/ls", "-l", "", "."));

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

TEST_F(ProcessTest, Run_Success) {
  EXPECT_CALL(process_, Start()).WillOnce(testing::Return(true));
  EXPECT_CALL(process_, Wait()).WillOnce(testing::Return(42));
  EXPECT_EQ(42, process_.Run());
}

TEST_F(ProcessTest, Run_Fail) {
  EXPECT_CALL(process_, Start()).WillOnce(testing::Return(false));
  EXPECT_CALL(process_, Wait()).Times(0);
  EXPECT_EQ(-1, process_.Run());
}

}  // namespace cros_disks
