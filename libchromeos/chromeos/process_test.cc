// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <chromeos/process.h>
#include <chromeos/test_helpers.h>

#include <base/file_util.h>
#include <gtest/gtest.h>

// This test assumes the following standard binaries are installed.
static const char kBinBash[] = "/bin/bash";
static const char kBinCp[] = "/bin/cp";
static const char kBinEcho[] = "/bin/echo";
static const char kBinFalse[] = "/bin/false";
static const char kBinSleep[] = "/bin/sleep";

using chromeos::Process;
using chromeos::ProcessImpl;
using chromeos::FindLog;
using chromeos::GetLog;

TEST(SimpleProcess, Basic) {
  ProcessImpl process;
  process.AddArg(kBinEcho);
  EXPECT_EQ(0, process.Run());
  EXPECT_EQ("", GetLog());
}

class ProcessTest : public ::testing::Test {
 public:
  void SetUp() {
    test_path_ = FilePath("test");
    output_file_ = test_path_.Append("fork_out").value();
    file_util::Delete(test_path_, true);
    file_util::CreateDirectory(test_path_);
    process_.RedirectOutput(output_file_);
    chromeos::ClearLog();
  }

  void TearDown() {
    file_util::Delete(test_path_, true);
  }

 protected:
  void CheckStderrCaptured();

  ProcessImpl process_;
  std::vector<const char*> args_;
  std::string output_file_;
  FilePath test_path_;
};

TEST_F(ProcessTest, Basic) {
  process_.AddArg(kBinEcho);
  process_.AddArg("hello world");
  EXPECT_EQ(0, process_.Run());
  ExpectFileEquals("hello world\n", output_file_.c_str());
  EXPECT_EQ("", GetLog());
}

TEST_F(ProcessTest, AddStringOption) {
  process_.AddArg(kBinEcho);
  process_.AddStringOption("--hello", "world");
  EXPECT_EQ(0, process_.Run());
  ExpectFileEquals("--hello world\n", output_file_.c_str());
}

TEST_F(ProcessTest, AddIntValue) {
  process_.AddArg(kBinEcho);
  process_.AddIntOption("--answer", 42);
  EXPECT_EQ(0, process_.Run());
  ExpectFileEquals("--answer 42\n", output_file_.c_str());
}

TEST_F(ProcessTest, NonZeroReturnValue) {
  process_.AddArg(kBinFalse);
  EXPECT_EQ(1, process_.Run());
  ExpectFileEquals("", output_file_.c_str());
  EXPECT_EQ("", GetLog());
}

TEST_F(ProcessTest, BadOutputFile) {
  process_.AddArg(kBinEcho);
  process_.RedirectOutput("/bad/path");
  EXPECT_EQ(127, process_.Run());
}

TEST_F(ProcessTest, ExistingOutputFile) {
  process_.AddArg(kBinEcho);
  process_.AddArg("hello world");
  EXPECT_FALSE(file_util::PathExists(FilePath(output_file_)));
  EXPECT_EQ(0, process_.Run());
  EXPECT_TRUE(file_util::PathExists(FilePath(output_file_)));
  EXPECT_EQ(127, process_.Run());
}

TEST_F(ProcessTest, BadExecutable) {
  process_.AddArg("false");
  EXPECT_EQ(127, process_.Run());
}

void ProcessTest::CheckStderrCaptured() {
  std::string contents;
  process_.AddArg(kBinCp);
  EXPECT_EQ(1, process_.Run());
  EXPECT_TRUE(file_util::ReadFileToString(FilePath(output_file_),
                                                   &contents));
  EXPECT_NE(std::string::npos, contents.find("missing file operand"));
  EXPECT_EQ("", GetLog());
}

TEST_F(ProcessTest, StderrCaptured) {
  CheckStderrCaptured();
}

TEST_F(ProcessTest, StderrCapturedWhenPreviouslyClosed) {
  int saved_stderr = dup(STDERR_FILENO);
  close(STDERR_FILENO);
  CheckStderrCaptured();
  dup2(saved_stderr, STDERR_FILENO);
}

TEST_F(ProcessTest, NoParams) {
  EXPECT_EQ(127, process_.Run());
}

TEST_F(ProcessTest, SegFaultHandling) {
  process_.AddArg(kBinBash);
  process_.AddArg("-c");
  process_.AddArg("kill -SEGV $$");
  EXPECT_EQ(-1, process_.Run());
  EXPECT_TRUE(FindLog("did not exit normally: 11"));
}

TEST_F(ProcessTest, KillNoPid) {
  process_.Kill(SIGTERM, 0);
  EXPECT_TRUE(FindLog("Process not running"));
}

TEST_F(ProcessTest, ProcessExists) {
  EXPECT_FALSE(Process::ProcessExists(0));
  EXPECT_TRUE(Process::ProcessExists(1));
  EXPECT_TRUE(Process::ProcessExists(getpid()));
}

TEST_F(ProcessTest, ResetPidByFile) {
  FilePath pid_path = test_path_.Append("pid");
  EXPECT_FALSE(process_.ResetPidByFile(pid_path.value()));
  EXPECT_TRUE(file_util::WriteFile(pid_path, "456\n", 4));
  EXPECT_TRUE(process_.ResetPidByFile(pid_path.value()));
  EXPECT_EQ(456, process_.pid());
}

TEST_F(ProcessTest, KillSleeper) {
  process_.AddArg(kBinSleep);
  process_.AddArg("10000");
  ASSERT_TRUE(process_.Start());
  pid_t pid = process_.pid();
  ASSERT_GT(pid, 1);
  EXPECT_TRUE(process_.Kill(SIGTERM, 1));
  EXPECT_EQ(0, process_.pid());
}

TEST_F(ProcessTest, Reset) {
  process_.AddArg(kBinFalse);
  process_.Reset(0);
  process_.AddArg(kBinEcho);
  EXPECT_EQ(0, process_.Run());
}
