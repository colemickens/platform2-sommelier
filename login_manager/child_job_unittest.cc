// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "login_manager/child_job.h"

#include <unistd.h>

#include <string>
#include <vector>

#include <base/command_line.h>
#include <base/logging.h>
#include <base/memory/scoped_ptr.h>
#include <base/string_util.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "login_manager/mock_system_utils.h"

using std::string;
using std::vector;

namespace login_manager {

using ::testing::Return;
using ::testing::_;

class ChildJobTest : public ::testing::Test {
 public:
  ChildJobTest() {}

  virtual ~ChildJobTest() {}

  virtual void SetUp();

  virtual void TearDown() {
  }

 protected:
  static const char* kArgv[];
  static const char kUser[];

  vector<string> argv_;
  MockSystemUtils utils_;
  scoped_ptr<ChildJob> job_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ChildJobTest);
};


// Default argument list for a job to use in mostly all test cases.
const char* ChildJobTest::kArgv[] = {
    "zero",
    "one",
    "two"
};

// Normal username to test session for.
const char ChildJobTest::kUser[] = "test@gmail.com";

void ChildJobTest::SetUp() {
  vector<string> argv(kArgv, kArgv + arraysize(ChildJobTest::kArgv));
  job_.reset(new ChildJob(argv, &utils_));
}

TEST_F(ChildJobTest, InitializationTest) {
  EXPECT_EQ(0, job_->last_start_);
  EXPECT_FALSE(job_->removed_login_manager_flag_);
  ASSERT_EQ(arraysize(kArgv), job_->arguments_.size());
  for (size_t i = 0; i < arraysize(kArgv); ++i)
    EXPECT_EQ(kArgv[i], job_->arguments_[i]);
  EXPECT_FALSE(job_->IsDesiredUidSet());
}

TEST_F(ChildJobTest, DesiredUidSetTest) {
  EXPECT_FALSE(job_->IsDesiredUidSet());
  job_->SetDesiredUid(1);
  EXPECT_EQ(1, job_->GetDesiredUid());
  EXPECT_TRUE(job_->IsDesiredUidSet());
}

TEST_F(ChildJobTest, ShouldStopTest) {
  EXPECT_CALL(utils_, time(NULL))
      .WillOnce(Return(ChildJob::kRestartWindow))
      .WillOnce(Return(ChildJob::kRestartWindow));
  job_->RecordTime();
  EXPECT_NE(0, job_->last_start_);
  EXPECT_TRUE(job_->ShouldStop());
}

TEST_F(ChildJobTest, ShouldNotStopTest) {
  EXPECT_CALL(utils_, time(NULL))
      .WillOnce(Return(ChildJob::kRestartWindow))
      .WillOnce(Return(3 * ChildJob::kRestartWindow));
  job_->RecordTime();
  EXPECT_NE(0, job_->last_start_);
  EXPECT_FALSE(job_->ShouldStop());
}

TEST_F(ChildJobTest, StartStopSessionTest) {
  job_->StartSession(kUser);
  EXPECT_FALSE(job_->removed_login_manager_flag_);
  ASSERT_EQ(arraysize(kArgv) + 1, job_->arguments_.size());
  for (size_t i = 0; i < arraysize(kArgv); ++i)
    EXPECT_EQ(kArgv[i], job_->arguments_[i]);
  EXPECT_EQ(StringPrintf("%s%s", ChildJob::kLoginUserFlag, kUser),
            job_->arguments_[arraysize(kArgv)]);

  // Should remove login user flag.
  job_->StopSession();
  ASSERT_EQ(arraysize(kArgv), job_->arguments_.size());
  for (size_t i = 0; i < arraysize(kArgv); ++i)
    EXPECT_EQ(kArgv[i], job_->arguments_[i]);
}

TEST_F(ChildJobTest, StartStopSessionFromLoginTest) {
  const char* kArgvWithLoginFlag[] = {
      "zero",
      "one",
      "two",
      "--login-manager"
  };
  vector<string> argv(kArgvWithLoginFlag,
                      kArgvWithLoginFlag + arraysize(kArgvWithLoginFlag));
  ChildJob job(argv, &utils_);

  job.StartSession(kUser);
  EXPECT_TRUE(job.removed_login_manager_flag_);
  ASSERT_EQ(arraysize(kArgvWithLoginFlag), job.arguments_.size());
  for (size_t i = 0; i + 1 < arraysize(kArgvWithLoginFlag); ++i)
    EXPECT_EQ(kArgvWithLoginFlag[i], job.arguments_[i]);
  EXPECT_EQ(StringPrintf("%s%s", ChildJob::kLoginUserFlag, kUser),
            job.arguments_.back());

  // Should remove login user flag and append --login-manager flag back.
  job.StopSession();
  EXPECT_FALSE(job.removed_login_manager_flag_);
  ASSERT_EQ(arraysize(kArgvWithLoginFlag), job.arguments_.size());
  for (size_t i = 0; i < arraysize(kArgvWithLoginFlag); ++i)
    EXPECT_EQ(kArgvWithLoginFlag[i], job.arguments_[i]);
}

TEST_F(ChildJobTest, SetArguments) {
  const char kNewArgsString[] = "--ichi \"--ni dfs\" --san";
  const char* kNewArgsArray[] = {
    "--ichi",
    "--ni dfs",
    "--san"
  };
  job_->SetArguments(kNewArgsString);

  ASSERT_EQ(arraysize(kNewArgsArray), job_->arguments_.size());
  EXPECT_EQ(kArgv[0], job_->arguments_[0]);
  for (size_t i = 1; i < arraysize(kNewArgsArray); ++i) {
    EXPECT_EQ(kNewArgsArray[i], job_->arguments_[i]);
  }
}

TEST_F(ChildJobTest, SetExtraArguments) {
  const char* kExtraArgs[] = { "--ichi", "--ni", "--san" };
  vector<string> extra_args(kExtraArgs, kExtraArgs + arraysize(kExtraArgs));
  job_->SetExtraArguments(extra_args);

  // Make sure regular arguments are untouched.
  ASSERT_EQ(arraysize(kArgv), job_->arguments_.size());
  for (size_t i = 0; i < arraysize(kArgv); ++i)
    EXPECT_EQ(kArgv[i], job_->arguments_[i]);

  ASSERT_EQ(extra_args.size(), job_->extra_arguments_.size());
  for (size_t i = 0; i < extra_args.size(); ++i)
    EXPECT_EQ(extra_args[i], job_->extra_arguments_[i]);
}

TEST_F(ChildJobTest, AddExtraOneTimeArgument) {
  std::string argument("--san");
  job_->AddOneTimeArgument(argument);

  // Make sure regular arguments are untouched.
  ASSERT_EQ(arraysize(kArgv), job_->arguments_.size());
  for (size_t i = 0; i < arraysize(kArgv); ++i)
    EXPECT_EQ(kArgv[i], job_->arguments_[i]);

  ASSERT_EQ(argument.size(), job_->extra_one_time_argument_.size());
  EXPECT_EQ(argument, job_->extra_one_time_argument_);
}

TEST_F(ChildJobTest, CreateArgv) {
  vector<string> argv(kArgv, kArgv + arraysize(kArgv));
  ChildJob job(argv, &utils_);

  const char* kExtraArgs[] = { "--ichi", "--ni", "--san" };
  vector<string> extra_args(kExtraArgs, kExtraArgs + arraysize(kExtraArgs));
  job.SetExtraArguments(extra_args);

  const char** arg_array = job.CreateArgv();

  argv.insert(argv.end(), extra_args.begin(), extra_args.end());

  size_t arg_array_size = 0;
  for (const char** arr = arg_array; *arr != NULL; ++arr)
    ++arg_array_size;

  ASSERT_EQ(argv.size(), arg_array_size);
  for (size_t i = 0; i < argv.size(); ++i)
    EXPECT_EQ(argv[i], arg_array[i]);
}

// Test that we avoid killing the window manager job.
TEST_F(ChildJobTest, AvoidKillingWindowManager) {
  vector<string> wm_args;
  wm_args.push_back(string("/sbin/") + ChildJob::kWindowManagerSuffix);
  wm_args.push_back("--flag1");
  wm_args.push_back("--flag2");
  ChildJob wm_job(wm_args, &utils_);
  EXPECT_TRUE(wm_job.ShouldNeverKill());

  vector<string> chrome_args;
  chrome_args.push_back("/opt/google/chrome/chrome");
  chrome_args.push_back("--flag1");
  chrome_args.push_back("--flag2");
  ChildJob chrome_job(chrome_args, &utils_);
  EXPECT_FALSE(chrome_job.ShouldNeverKill());
}

}  // namespace login_manager
