// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cros-disks/process.h"

#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/files/scoped_temp_dir.h>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "cros-disks/sandboxed_process.h"

namespace cros_disks {

using testing::_;
using testing::Contains;
using testing::ElementsAre;
using testing::Return;

// A mock process class for testing the process base class.
class ProcessUnderTest : public Process {
 public:
  ProcessUnderTest() = default;

  MOCK_METHOD(pid_t,
              StartImpl,
              (base::ScopedFD*, base::ScopedFD*, base::ScopedFD*),
              (override));
  MOCK_METHOD(int, WaitImpl, (), (override));
  MOCK_METHOD(bool, WaitNonBlockingImpl, (int*), (override));
};

class ProcessTest : public ::testing::Test {
 protected:
  ProcessUnderTest process_;
};

TEST_F(ProcessTest, GetArguments) {
  const char* const kTestArguments[] = {"/bin/ls", "-l", "", "."};
  for (const char* test_argument : kTestArguments) {
    process_.AddArgument(test_argument);
  }

  EXPECT_THAT(process_.arguments(), ElementsAre("/bin/ls", "-l", "", "."));

  char* const* arguments = process_.GetArguments();
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
  process_.AddArgument("foo");
  EXPECT_CALL(process_, StartImpl(_, _, _)).WillOnce(Return(123));
  EXPECT_CALL(process_, WaitImpl()).WillOnce(Return(42));
  EXPECT_CALL(process_, WaitNonBlockingImpl(_)).Times(0);
  EXPECT_EQ(42, process_.Run());
}

TEST_F(ProcessTest, Run_Fail) {
  process_.AddArgument("foo");
  EXPECT_CALL(process_, StartImpl(_, _, _)).WillOnce(Return(-1));
  EXPECT_CALL(process_, WaitImpl()).Times(0);
  EXPECT_CALL(process_, WaitNonBlockingImpl(_)).Times(0);
  EXPECT_EQ(-1, process_.Run());
}

TEST_F(ProcessTest, Communicate) {
  base::ScopedTempDir dir;
  ASSERT_TRUE(dir.CreateUniqueTempDir());

  std::string file1 = dir.GetPath().Append("file1").value();
  std::string file2 = dir.GetPath().Append("file2").value();
  std::string file3 = dir.GetPath().Append("file3").value();
  ASSERT_EQ(6, base::WriteFile(base::FilePath(file1), "data1\n", 6));
  ASSERT_EQ(6, base::WriteFile(base::FilePath(file2), "data2\n", 6));
  ASSERT_EQ(6, base::WriteFile(base::FilePath(file3), "data3\n", 6));
  ASSERT_EQ(0, chmod(file2.c_str(), 0));

  // Using SandboxedProcess as it's the only real implementation.
  SandboxedProcess sandbox;
  Process* process = &sandbox;

  process->AddArgument("/bin/cat");
  process->AddArgument(file1);
  process->AddArgument(file2);
  process->AddArgument(file3);

  ASSERT_TRUE(process->Start());
  std::vector<std::string> output;
  process->Communicate(&output);
  EXPECT_THAT(output, Contains("OUT: data1"));
  EXPECT_THAT(output, Contains("OUT: data3"));
  EXPECT_THAT(output, Contains("ERR: cat: " + file2 + ": Permission denied"));
  EXPECT_THAT(output, testing::Not(Contains("OUT: data2")));

  process->Wait();
}

}  // namespace cros_disks
