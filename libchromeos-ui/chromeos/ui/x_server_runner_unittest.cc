// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ui/x_server_runner.h"

#include <fcntl.h>
#include <pwd.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <base/bind.h>
#include <base/callback.h>
#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/files/scoped_temp_dir.h>
#include <base/macros.h>
#include <base/posix/eintr_wrapper.h>
#include <base/strings/string_number_conversions.h>
#include <base/threading/platform_thread.h>
#include <gtest/gtest.h>

#include "chromeos/ui/util.h"

namespace chromeos {
namespace ui {

namespace {

// Passed to XServerRunner as a callback that should be run instead of actually
// starting the X server. Writes the current process's PID to |pipe_path|. If
// |exit_delay| is non-zero, sleeps and exits without sending SIGUSR1.
// Otherwise, sleeps for |signal_delay|, sends SIGUSR1 to its parent process,
// and then sleeps for a long time.
void ExecServer(const base::FilePath& pipe_path,
                const base::TimeDelta& signal_delay,
                const base::TimeDelta& exit_delay) {
  // Write our PID so the test (our grandparent process) can clean us up.
  pid_t pid = getpid();
  PCHECK(base::WriteFile(pipe_path, reinterpret_cast<const char*>(&pid),
                         sizeof(pid)) == sizeof(pid));

  // Check that the child process didn't inherit any blocked signals:
  // http://crbug.com/380713
  sigset_t old_signals;
  PCHECK(sigemptyset(&old_signals) == 0);
  PCHECK(sigprocmask(SIG_SETMASK, nullptr, &old_signals) == 0);
  CHECK(sigisemptyset(&old_signals)) << "Child inherited blocked signals";

  if (exit_delay > base::TimeDelta()) {
    base::PlatformThread::Sleep(exit_delay);
    exit(1);
  }

  if (signal_delay > base::TimeDelta())
    base::PlatformThread::Sleep(signal_delay);
  PCHECK(kill(getppid(), SIGUSR1) == 0);

  base::PlatformThread::Sleep(base::TimeDelta::FromSeconds(60));
}

}  // namespace

class XServerRunnerTest : public testing::Test {
 public:
  XServerRunnerTest() : server_pid_(0) {
    CHECK(temp_dir_.CreateUniqueTempDir());
    base_path_ = temp_dir_.path();
    runner_.set_base_path_for_testing(base_path_);
    xauth_path_ = base_path_.Append("xauth");
  }
  virtual ~XServerRunnerTest() {}

  // Calls StartServer(). See ExecServer() for descriptions of the arguments.
  void StartServer(const base::TimeDelta& signal_delay,
                   const base::TimeDelta& exit_delay) {
    // Named pipe used by ExecServer() to pass its PID back to the test process.
    base::FilePath pipe_path = base_path_.Append("pipe");
    PCHECK(mkfifo(pipe_path.value().c_str(), 0600) == 0);

    runner_.set_callback_for_testing(
        base::Bind(&ExecServer, pipe_path, signal_delay, exit_delay));
    passwd* user_info = getpwuid(getuid());
    ASSERT_TRUE(user_info) << "getpwuid() didn't find UID " << getuid();
    ASSERT_TRUE(runner_.StartServer(user_info->pw_name, 1, false, xauth_path_));

    // Open the pipe and read ExecServer()'s PID.
    int pipe_fd = open(pipe_path.value().c_str(), O_RDONLY);
    PCHECK(pipe_fd >= 0) << "Failed to open " << pipe_path.value();
    PCHECK(HANDLE_EINTR(read(pipe_fd, &server_pid_, sizeof(server_pid_))) ==
           sizeof(server_pid_));
    close(pipe_fd);
  }

  // Calls WaitForServer() and returns its result. If it returns true (i.e. the
  // X server process sent SIGUSR1), additionally kills the process before
  // returning.
  bool WaitForServer() {
    // No need to kill the process if it already exited on its own.
    if (!runner_.WaitForServer())
      return false;

    LOG(INFO) << "Killing server process " << server_pid_;
    kill(server_pid_, SIGTERM);
    return true;
  }

 protected:
  base::ScopedTempDir temp_dir_;
  base::FilePath base_path_;
  base::FilePath xauth_path_;

  XServerRunner runner_;

  // PID of the process running ExecServer().
  pid_t server_pid_;

 private:
  DISALLOW_COPY_AND_ASSIGN(XServerRunnerTest);
};

TEST_F(XServerRunnerTest, FastSuccess) {
  StartServer(base::TimeDelta(), base::TimeDelta());
  EXPECT_TRUE(WaitForServer());
}

TEST_F(XServerRunnerTest, SlowSuccess) {
  StartServer(base::TimeDelta::FromSeconds(1), base::TimeDelta());
  EXPECT_TRUE(WaitForServer());
}

TEST_F(XServerRunnerTest, FastCrash) {
  StartServer(base::TimeDelta(), base::TimeDelta::FromMicroseconds(1));
  EXPECT_FALSE(WaitForServer());
}

TEST_F(XServerRunnerTest, SlowCrash) {
  StartServer(base::TimeDelta(), base::TimeDelta::FromSeconds(1));
  EXPECT_FALSE(WaitForServer());
}

TEST_F(XServerRunnerTest, TermServer) {
  StartServer(base::TimeDelta::FromSeconds(60), base::TimeDelta());
  PCHECK(kill(server_pid_, SIGTERM) == 0);
  EXPECT_FALSE(WaitForServer());
}

TEST_F(XServerRunnerTest, StopAndContinueServer) {
  // Test that SIGCHLD signals that are sent in response to the process being
  // stopped or continued are ignored.
  StartServer(base::TimeDelta::FromSeconds(1), base::TimeDelta());
  PCHECK(kill(server_pid_, SIGSTOP) == 0);
  base::PlatformThread::Sleep(base::TimeDelta::FromMilliseconds(100));
  PCHECK(kill(server_pid_, SIGCONT) == 0);
  EXPECT_TRUE(WaitForServer());
}

TEST_F(XServerRunnerTest, XauthFile) {
  StartServer(base::TimeDelta(), base::TimeDelta());
  EXPECT_TRUE(WaitForServer());

  std::string data;
  ASSERT_TRUE(base::ReadFileToString(xauth_path_, &data));

  const char kExpected[] =
      "\x01" "\x00"
      "\x00" "\x09" "localhost"
      "\x00" "\x01" "0"
      "\x00" "\x12" "MIT-MAGIC-COOKIE-1"
      "\x00" "\x10" /* random 16-byte cookie data goes here */;
  const size_t kExpectedSize = arraysize(kExpected) - 1;
  const size_t kCookieSize = 16;

  ASSERT_EQ(kExpectedSize + kCookieSize, data.size());
  EXPECT_EQ(base::HexEncode(kExpected, kExpectedSize),
            base::HexEncode(data.data(), kExpectedSize));
}

TEST_F(XServerRunnerTest, CreateDirectories) {
  StartServer(base::TimeDelta(), base::TimeDelta());
  EXPECT_TRUE(WaitForServer());

  EXPECT_TRUE(base::DirectoryExists(util::GetReparentedPath(
      XServerRunner::kSocketDir, base_path_)));
  EXPECT_TRUE(base::DirectoryExists(util::GetReparentedPath(
      XServerRunner::kIceDir, base_path_)));
  EXPECT_TRUE(base::DirectoryExists(util::GetReparentedPath(
      XServerRunner::kXkbDir, base_path_)));

  base::FilePath log_file(util::GetReparentedPath(
      XServerRunner::kLogFile, base_path_));
  base::FilePath log_dir(log_file.DirName());
  EXPECT_TRUE(base::DirectoryExists(log_dir));

  // Check that a relative symlink is created in the directory above the one
  // where the log file is written.
  base::FilePath link;
  EXPECT_TRUE(base::ReadSymbolicLink(
      log_dir.DirName().Append(log_file.BaseName()), &link));
  EXPECT_EQ(log_dir.BaseName().Append(log_file.BaseName()).value(),
            link.value());
}

}  // namespace ui
}  // namespace chromeos
