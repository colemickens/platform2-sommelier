// Copyright (c) 2009-2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "login_manager/child_job.h"

#include <gtest/gtest.h>
#include <unistd.h>

#include <string>
#include <vector>

#include <base/command_line.h>
#include <base/logging.h>
#include <base/string_util.h>

using std::string;
using std::vector;

namespace login_manager {

namespace {

// Default argument list for a job to use in mostly all test cases.
const char* kArgv[] = {
    "zero",
    "one",
    "two"
};

// Normal username to test session for.
const char kUser[] = "test@gmail.com";

// Username which BWSI session is started for.
const char kBWSIUser[] = "";

}  // namespace

TEST(ChildJobTest, InitializationTest) {
  vector<string> argv(kArgv, kArgv + arraysize(kArgv));
  ChildJob job(argv);

  EXPECT_EQ(0, job.last_start_);
  EXPECT_FALSE(job.removed_login_manager_flag_);
  ASSERT_EQ(arraysize(kArgv), job.arguments_.size());
  for (size_t i = 0; i < arraysize(kArgv); ++i)
    EXPECT_EQ(kArgv[i], job.arguments_[i]);
  EXPECT_FALSE(job.IsDesiredUidSet());
}

TEST(ChildJobTest, DesiredUidSetTest) {
  vector<string> argv(kArgv, kArgv + arraysize(kArgv));
  ChildJob job(argv);
  EXPECT_FALSE(job.IsDesiredUidSet());
  job.SetDesiredUid(1);
  EXPECT_EQ(1, job.GetDesiredUid());
  EXPECT_TRUE(job.IsDesiredUidSet());
}

TEST(ChildJobTest, ShouldStopTest) {
  vector<string> argv(kArgv, kArgv + arraysize(kArgv));
  ChildJob job(argv);
  job.RecordTime();
  EXPECT_NE(0, job.last_start_);
  EXPECT_TRUE(job.ShouldStop());
}

TEST(ChildJobTest, ShouldNotStopTest) {
  vector<string> argv(kArgv, kArgv + arraysize(kArgv));
  ChildJob job(argv);
  job.RecordTime();
  EXPECT_NE(0, job.last_start_);
  sleep(ChildJob::kRestartWindow);
  EXPECT_FALSE(job.ShouldStop());
}

TEST(ChildJobTest, StartStopSessionTest) {
  vector<string> argv(kArgv, kArgv + arraysize(kArgv));
  ChildJob job(argv);

  job.StartSession(kUser);
  EXPECT_FALSE(job.removed_login_manager_flag_);
  ASSERT_EQ(arraysize(kArgv) + 1, job.arguments_.size());
  for (size_t i = 0; i < arraysize(kArgv); ++i)
    EXPECT_EQ(kArgv[i], job.arguments_[i]);
  EXPECT_EQ(StringPrintf("%s%s", ChildJob::kLoginUserFlag, kUser),
            job.arguments_[arraysize(kArgv)]);

  // Should remove login user flag.
  job.StopSession();
  ASSERT_EQ(arraysize(kArgv), job.arguments_.size());
  for (size_t i = 0; i < arraysize(kArgv); ++i)
    EXPECT_EQ(kArgv[i], job.arguments_[i]);
}

TEST(ChildJobTest, StartStopBWSISessionTest) {
  vector<string> argv(kArgv, kArgv + arraysize(kArgv));
  ChildJob job(argv);

  job.StartSession(kBWSIUser);
  EXPECT_FALSE(job.removed_login_manager_flag_);
  ASSERT_EQ(arraysize(kArgv) + 2, job.arguments_.size());
  for (size_t i = 0; i < arraysize(kArgv); ++i)
    EXPECT_EQ(kArgv[i], job.arguments_[i]);
  EXPECT_EQ(ChildJob::kBWSIFlag, job.arguments_[arraysize(kArgv)]);
  EXPECT_EQ(StringPrintf("%s%s", ChildJob::kLoginUserFlag, kBWSIUser),
            job.arguments_[arraysize(kArgv) + 1]);

  // Should remove bwsi login user flag and bwsi flag.
  job.StopSession();
  ASSERT_EQ(arraysize(kArgv), job.arguments_.size());
  for (size_t i = 0; i < arraysize(kArgv); ++i)
    EXPECT_EQ(kArgv[i], job.arguments_[i]);
}

TEST(ChildJobTest, StartStopSessionFromLoginTest) {
  const char* kArgvWithLoginFlag[] = {
      "zero",
      "one",
      "two",
      "--login-manager"
  };
  vector<string> argv(kArgvWithLoginFlag,
                      kArgvWithLoginFlag + arraysize(kArgvWithLoginFlag));
  ChildJob job(argv);

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

}  // namespace login_manager
