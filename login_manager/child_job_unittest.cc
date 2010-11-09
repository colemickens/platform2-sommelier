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

TEST(ChildJobTest, SetArguments) {
  vector<string> argv(kArgv, kArgv + arraysize(kArgv));
  ChildJob job(argv);

  const char kNewArgsString[] = "--ichi \"--ni dfs\" --san";
  const char* kNewArgsArray[] = {
    "--ichi",
    "--ni dfs",
    "--san"
  };
  job.SetArguments(kNewArgsString);

  ASSERT_EQ(arraysize(kNewArgsArray), job.arguments_.size());
  EXPECT_EQ(argv[0], job.arguments_[0]);
  for (size_t i = 1; i < arraysize(kNewArgsArray); ++i) {
    EXPECT_EQ(kNewArgsArray[i], job.arguments_[i]);
  }
}

// Test that we avoid killing the window manager job.
TEST(ChildJobTest, AvoidKillingWindowManager) {
  vector<string> wm_args;
  wm_args.push_back(string("/sbin/") + ChildJob::kWindowManagerSuffix);
  wm_args.push_back("--flag1");
  wm_args.push_back("--flag2");
  ChildJob wm_job(wm_args);
  EXPECT_TRUE(wm_job.ShouldNeverKill());

  vector<string> chrome_args;
  chrome_args.push_back("/opt/google/chrome/chrome");
  chrome_args.push_back("--flag1");
  chrome_args.push_back("--flag2");
  ChildJob chrome_job(chrome_args);
  EXPECT_FALSE(chrome_job.ShouldNeverKill());
}

}  // namespace login_manager
