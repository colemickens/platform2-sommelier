// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "login_manager/child_job.h"

#include <unistd.h>

#include <algorithm>
#include <set>
#include <string>
#include <vector>

#include <base/command_line.h>
#include <base/logging.h>
#include <base/memory/scoped_ptr.h>
#include <base/string_util.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "login_manager/mock_system_utils.h"

namespace login_manager {

using ::testing::Return;
using ::testing::_;

class ChildJobTest : public ::testing::Test {
 public:
  ChildJobTest() {}

  virtual ~ChildJobTest() {}

  virtual void SetUp() OVERRIDE;

 protected:
  static const char* kArgv[];
  static const char kUser[];
  static const char kHash[];

  void ExpectArgsToContainFlag(const std::vector<std::string>& argv,
                               const char name[],
                               const char value[]) {
    std::vector<std::string>::const_iterator user_flag =
        std::find(argv.begin(), argv.end(), StringPrintf("%s%s", name, value));
    EXPECT_NE(user_flag, argv.end()) << "argv should contain " << name << value;
  }

  void ExpectArgsToContainAll(const std::vector<std::string>& argv,
                              const std::vector<std::string>& contained) {
    std::set<std::string> argv_set(argv.begin(), argv.end());
    for (std::vector<std::string>::const_iterator it = contained.begin();
         it != contained.end();
         ++it) {
      EXPECT_EQ(argv_set.count(*it), 1) << "argv should contain " << *it;
    }
  }

  std::vector<std::string> argv_;
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
const char ChildJobTest::kHash[] = "fake_hash";

void ChildJobTest::SetUp() {
  argv_ = std::vector<std::string>(kArgv,
                                   kArgv + arraysize(ChildJobTest::kArgv));
  job_.reset(new ChildJob(argv_, false, &utils_));
}

TEST_F(ChildJobTest, InitializationTest) {

  EXPECT_FALSE(job_->removed_login_manager_flag_);
  EXPECT_FALSE(job_->IsDesiredUidSet());
  std::vector<std::string> job_args = job_->ExportArgv();
  ASSERT_EQ(argv_.size(), job_args.size());
  ExpectArgsToContainAll(job_args, argv_);
}

TEST_F(ChildJobTest, DesiredUidSetTest) {
  EXPECT_FALSE(job_->IsDesiredUidSet());
  job_->SetDesiredUid(1);
  EXPECT_EQ(1, job_->GetDesiredUid());
  EXPECT_TRUE(job_->IsDesiredUidSet());
}

TEST_F(ChildJobTest, ShouldStopTest) {
  EXPECT_CALL(utils_, time(NULL))
      .WillRepeatedly(Return(ChildJob::kRestartWindowSeconds));
  for (uint i = 0; i < ChildJob::kRestartTries - 1; ++i)
    job_->RecordTime();
  // We haven't yet saturated the list of start times, so...
  EXPECT_FALSE(job_->ShouldStop());

  // Go ahead and saturate.
  job_->RecordTime();
  EXPECT_NE(0, job_->start_times_.front());
  EXPECT_TRUE(job_->ShouldStop());
}

TEST_F(ChildJobTest, ShouldNotStopTest) {
  EXPECT_CALL(utils_, time(NULL))
      .WillOnce(Return(ChildJob::kRestartWindowSeconds))
      .WillOnce(Return(3 * ChildJob::kRestartWindowSeconds));
  job_->RecordTime();
  EXPECT_FALSE(job_->ShouldStop());
}

TEST_F(ChildJobTest, StartStopSessionTest) {
  job_->StartSession(kUser, kHash);

  std::vector<std::string> job_args = job_->ExportArgv();
  ASSERT_LT(argv_.size(), job_args.size());
  ExpectArgsToContainAll(job_args, argv_);
  ExpectArgsToContainFlag(job_args, ChildJob::kLoginUserFlag, kUser);
  ExpectArgsToContainFlag(job_args, ChildJob::kLoginProfileFlag, "user");

  // Should remove login user flag.
  job_->StopSession();
  job_args = job_->ExportArgv();
  ASSERT_EQ(argv_.size(), job_args.size());
  ExpectArgsToContainAll(job_args, argv_);
}

TEST_F(ChildJobTest, StartStopMultiSessionTest) {
  ChildJob job(argv_, true, &utils_);
  job.StartSession(kUser, kHash);

  std::vector<std::string> job_args = job.ExportArgv();
  ASSERT_EQ(argv_.size() + 3, job_args.size());
  ExpectArgsToContainAll(job_args, argv_);
  ExpectArgsToContainFlag(job_args, ChildJob::kLoginUserFlag, kUser);
  ExpectArgsToContainFlag(job_args, ChildJob::kLoginProfileFlag, kHash);

  // Start another session, expect the args to be unchanged.
  job.StartSession(kUser, kHash);
  job_args = job.ExportArgv();
  ASSERT_EQ(argv_.size() + 3, job_args.size());
  ExpectArgsToContainAll(job_args, argv_);
  ExpectArgsToContainFlag(job_args, ChildJob::kLoginUserFlag, kUser);
  ExpectArgsToContainFlag(job_args, ChildJob::kLoginProfileFlag, kHash);


  // Should remove login user and login profile flags.
  job.StopSession();
  job_args = job.ExportArgv();
  ASSERT_EQ(argv_.size() + 1, job_args.size());
  ExpectArgsToContainAll(job_args, argv_);
  ExpectArgsToContainFlag(job_args, ChildJob::kMultiProfileFlag, "");
}

TEST_F(ChildJobTest, StartStopSessionFromLoginTest) {
  const char* kArgvWithLoginFlag[] = {
      "zero",
      "one",
      "two",
      "--login-manager"
  };
  std::vector<std::string> argv(
      kArgvWithLoginFlag, kArgvWithLoginFlag + arraysize(kArgvWithLoginFlag));
  ChildJob job(argv, false, &utils_);

  job.StartSession(kUser, kHash);

  std::vector<std::string> job_args = job.ExportArgv();
  ASSERT_EQ(argv.size() + 1, job_args.size());
  ExpectArgsToContainAll(job_args,
                         std::vector<std::string>(argv.begin(), argv.end()-1));
  ExpectArgsToContainFlag(job_args, ChildJob::kLoginUserFlag, kUser);

  // Should remove login user/hash flags and append --login-manager flag back.
  job.StopSession();
  job_args = job.ExportArgv();
  ASSERT_EQ(argv.size(), job_args.size());
  ExpectArgsToContainAll(job_args, argv);
}

TEST_F(ChildJobTest, SetArguments) {
  const char* kNewArgs[] = {
    "--ichi",
    "--ni dfs",
    "--san"
  };
  std::vector<std::string> new_args(kNewArgs, kNewArgs + arraysize(kNewArgs));
  job_->SetArguments(new_args);

  std::vector<std::string> job_args = job_->ExportArgv();
  ASSERT_EQ(new_args.size(), job_args.size());
  EXPECT_EQ(kArgv[0], job_args[0]);
  for (size_t i = 1; i < arraysize(kNewArgs); ++i) {
    EXPECT_EQ(kNewArgs[i], job_args[i]);
  }

  job_->StartSession(kUser, kHash);
  job_args = job_->ExportArgv();
  ExpectArgsToContainFlag(job_args, ChildJob::kLoginUserFlag, kUser);
}

TEST_F(ChildJobTest, SetExtraArguments) {
  const char* kExtraArgs[] = { "--ichi", "--ni", "--san" };
  std::vector<std::string> extra_args(kExtraArgs,
                                      kExtraArgs + arraysize(kExtraArgs));
  job_->SetExtraArguments(extra_args);

  std::vector<std::string> job_args = job_->ExportArgv();
  ExpectArgsToContainAll(job_args, argv_);
  ExpectArgsToContainAll(job_args, extra_args);
}

TEST_F(ChildJobTest, AddExtraOneTimeArgument) {
  std::string argument("--san");
  job_->AddOneTimeArgument(argument);

  std::vector<std::string> job_args = job_->ExportArgv();
  ExpectArgsToContainAll(job_args, argv_);
  ExpectArgsToContainFlag(job_args, argument.c_str(), "");
}

TEST_F(ChildJobTest, CreateArgv) {
  std::vector<std::string> argv(kArgv, kArgv + arraysize(kArgv));
  ChildJob job(argv, false, &utils_);

  const char* kExtraArgs[] = { "--ichi", "--ni", "--san" };
  std::vector<std::string> extra_args(kExtraArgs,
                                      kExtraArgs + arraysize(kExtraArgs));
  job.SetExtraArguments(extra_args);

  const char** arg_array = job.CreateArgv();

  argv.insert(argv.end(), extra_args.begin(), extra_args.end());

  size_t arg_array_size = 0;
  for (const char** arr = arg_array; *arr != NULL; ++arr)
    ++arg_array_size;

  ASSERT_EQ(argv.size(), arg_array_size);
  for (size_t i = 0; i < argv.size(); ++i) {
    EXPECT_EQ(argv[i], arg_array[i]);
    delete [] arg_array[i];
  }
  delete [] arg_array;
}

}  // namespace login_manager
