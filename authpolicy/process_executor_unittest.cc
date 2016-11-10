// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/strings/string_util.h"

#include <authpolicy/process_executor.h>

namespace {

const char kCmdDoesNotExist[] = "/bin/does_not_exit_khsdgviu";
const char kCmdPrintEnv[] = "/usr/bin/printenv";
const char kCmdEcho[] = "/bin/echo";
const char kCmdGrep[] = "/bin/grep";
const char kCmdFalse[] = "/bin/false";
const char kEnvVar[] = "PROCESS_EXECUTOR_TEST_ENV_VAR";
const char kGrepTestText[] = "This is a test.\n";
const char kGrepTestToken[] = "test";

}  // namespace

namespace authpolicy {

class ProcessExecutorTest : public ::testing::Test {
 public:
  ProcessExecutorTest() {}
  ~ProcessExecutorTest() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(ProcessExecutorTest);
};

// Calling Execute() on an instance with no command args should succeed.
TEST_F(ProcessExecutorTest, EmptyArgs) {
  ProcessExecutor cmd = ProcessExecutor::Create({});
  EXPECT_TRUE(cmd.Execute());
  EXPECT_EQ(cmd.GetExitCode(), 0);
  EXPECT_TRUE(cmd.GetStdout().empty());
  EXPECT_TRUE(cmd.GetStderr().empty());
}

// Execute command with no additional args.
TEST_F(ProcessExecutorTest, CommandWithNoArgs) {
  ProcessExecutor cmd = ProcessExecutor::Create({kCmdEcho});
  EXPECT_TRUE(cmd.Execute());
  EXPECT_EQ(cmd.GetExitCode(), 0);
  EXPECT_FALSE(cmd.GetStdout().empty());
  EXPECT_TRUE(cmd.GetStderr().empty());
}

// Executing non-existing command should result in error in stderr.
TEST_F(ProcessExecutorTest, NonExistingCommand) {
  ProcessExecutor cmd = ProcessExecutor::Create({kCmdDoesNotExist});
  EXPECT_FALSE(cmd.Execute());
  EXPECT_NE(cmd.GetExitCode(), 0);
  EXPECT_TRUE(cmd.GetStdout().empty());
  EXPECT_TRUE(base::StartsWith(cmd.GetStderr(),
                               "LaunchProcess: failed to execvp:",
                               base::CompareCase::SENSITIVE));
}

// Repeated execution should have no side effects on stdout.
TEST_F(ProcessExecutorTest, RepeatedExecutionWorks_Stdout) {
  ProcessExecutor cmd = ProcessExecutor::Create({kCmdPrintEnv, kEnvVar});
  cmd.SetEnv(kEnvVar, "first");
  EXPECT_TRUE(cmd.Execute());
  EXPECT_EQ(cmd.GetExitCode(), 0);
  EXPECT_EQ(cmd.GetStdout(), "first\n");
  EXPECT_TRUE(cmd.GetStderr().empty());

  cmd.SetEnv(kEnvVar, "second");
  EXPECT_TRUE(cmd.Execute());
  EXPECT_EQ(cmd.GetExitCode(), 0);
  EXPECT_EQ(cmd.GetStdout(), "second\n");
  EXPECT_TRUE(cmd.GetStderr().empty());
}  // namespace authpolicy

// Repeated execution should have no side effects on stderr.
TEST_F(ProcessExecutorTest, RepeatedExecutionWorks_Stderr) {
  ProcessExecutor cmd = ProcessExecutor::Create({kCmdDoesNotExist});
  EXPECT_FALSE(cmd.Execute());
  EXPECT_NE(cmd.GetExitCode(), 0);
  EXPECT_TRUE(cmd.GetStdout().empty());
  std::string stderr = cmd.GetStderr();  // Important: Make copy!
  EXPECT_FALSE(stderr.empty());

  EXPECT_FALSE(cmd.Execute());
  EXPECT_NE(cmd.GetExitCode(), 0);
  EXPECT_TRUE(cmd.GetStdout().empty());
  EXPECT_EQ(cmd.GetStderr(), stderr);
}

// Reading output from stdout.
TEST_F(ProcessExecutorTest, ReadFromStdout) {
  ProcessExecutor cmd = ProcessExecutor::Create({kCmdEcho, "test"});
  EXPECT_TRUE(cmd.Execute());
  EXPECT_EQ(cmd.GetExitCode(), 0);
  EXPECT_EQ(cmd.GetStdout(), "test\n");
  EXPECT_TRUE(cmd.GetStderr().empty());
}

// Reading output from stderr.
TEST_F(ProcessExecutorTest, ReadFromStderr) {
  ProcessExecutor cmd = ProcessExecutor::Create({kCmdGrep, "--invalid_arg"});
  EXPECT_FALSE(cmd.Execute());
  EXPECT_NE(cmd.GetExitCode(), 0);
  EXPECT_TRUE(cmd.GetStdout().empty());
  EXPECT_TRUE(base::StartsWith(cmd.GetStderr(), kCmdGrep,
                               base::CompareCase::SENSITIVE));
}

// Getting exit codes.
TEST_F(ProcessExecutorTest, GetExitCode) {
  ProcessExecutor cmd = ProcessExecutor::Create({kCmdFalse});
  EXPECT_FALSE(cmd.Execute());
  EXPECT_EQ(cmd.GetExitCode(), 1);
}

// Setting input file.
TEST_F(ProcessExecutorTest, SetInputFile) {
  int input_pipes[2];
  EXPECT_TRUE(base::CreateLocalNonBlockingPipe(input_pipes));
  base::ScopedFD stdin_read_end(input_pipes[0]);
  base::ScopedFD stdin_write_end(input_pipes[1]);
  size_t num_chars = strlen(kGrepTestText);
  EXPECT_EQ(write(stdin_write_end.get(), kGrepTestText, num_chars), num_chars);
  stdin_write_end.reset();
  // Note: grep reads from stdin if no file arg is specified.
  ProcessExecutor cmd = ProcessExecutor::Create({kCmdGrep, kGrepTestToken});
  cmd.SetInputFile(stdin_read_end.get());
  EXPECT_TRUE(cmd.Execute());
  EXPECT_EQ(cmd.GetExitCode(), 0);
  EXPECT_EQ(cmd.GetStdout(), kGrepTestText);
  EXPECT_TRUE(cmd.GetStderr().empty());
}

// Setting an invalid input file results in an error code, but no error message.
TEST_F(ProcessExecutorTest, SetInvalidInputFile) {
  ProcessExecutor cmd = ProcessExecutor::Create({kCmdEcho, "test"});
  cmd.SetInputFile(-3);
  EXPECT_FALSE(cmd.Execute());
  EXPECT_EQ(cmd.GetExitCode(), 127);
  EXPECT_TRUE(cmd.GetStdout().empty());
  EXPECT_TRUE(cmd.GetStderr().empty());
}

// Setting an environment variable.
TEST_F(ProcessExecutorTest, SetEnvVariable) {
  ProcessExecutor cmd = ProcessExecutor::Create({kCmdPrintEnv, kEnvVar});
  cmd.SetEnv(kEnvVar, "test");
  EXPECT_TRUE(cmd.Execute());
  EXPECT_EQ(cmd.GetExitCode(), 0);
  EXPECT_EQ(cmd.GetStdout(), "test\n");
  EXPECT_TRUE(cmd.GetStderr().empty());
}

// List of environment variables is empty by default.
TEST_F(ProcessExecutorTest, NoEnvVariables) {
  ProcessExecutor cmd = ProcessExecutor::Create({kCmdPrintEnv});
  EXPECT_TRUE(cmd.Execute());
  EXPECT_EQ(cmd.GetExitCode(), 0);
  EXPECT_TRUE(cmd.GetStdout().empty());
  EXPECT_TRUE(cmd.GetStderr().empty());
}

// Make sure you can't inject arbitrary commands in args
TEST_F(ProcessExecutorTest, NoSideEffects) {
  ProcessExecutor cmd = ProcessExecutor::Create({kCmdEcho, "test; ls"});
  EXPECT_TRUE(cmd.Execute());
  EXPECT_EQ(cmd.GetExitCode(), 0);
  EXPECT_EQ(cmd.GetStdout(), "test; ls\n");
  EXPECT_TRUE(cmd.GetStderr().empty());
}

// Commands must start with /
TEST_F(ProcessExecutorTest, CommandsMustUseAbsolutePaths) {
  ProcessExecutor cmd = ProcessExecutor::Create({"echo", "test"});
  EXPECT_FALSE(cmd.Execute());
}

}  // namespace authpolicy
