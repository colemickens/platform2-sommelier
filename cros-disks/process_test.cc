// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cros-disks/process.h"

#include <csignal>
#include <memory>
#include <ostream>
#include <utility>

#include <fcntl.h>
#include <sys/time.h>
#include <unistd.h>

#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/files/scoped_file.h>
#include <base/logging.h>
#include <base/strings/string_piece.h>
#include <base/strings/stringprintf.h>
#include <chromeos/libminijail.h>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "cros-disks/sandboxed_process.h"

namespace cros_disks {
namespace {

using testing::_;
using testing::Contains;
using testing::ElementsAre;
using testing::PrintToStringParamName;
using testing::Return;
using testing::SizeIs;
using testing::StartsWith;
using testing::UnorderedElementsAre;
using testing::Values;

// Sets a signal handler for SIGALRM and an interval timer signaling SIGALRM at
// regular intervals.
class AlarmGuard {
 public:
  explicit AlarmGuard(const int timer_interval_ms) {
    CHECK(!old_handler_);
    count_ = 0;
    old_handler_ = signal(SIGALRM, &Handler);
    CHECK_NE(old_handler_, SIG_ERR);
    SetIntervalTimer(timer_interval_ms * 1000 /* microseconds */);
  }

  ~AlarmGuard() {
    SetIntervalTimer(0);
    CHECK_EQ(signal(SIGALRM, old_handler_), &Handler);
    old_handler_ = nullptr;
  }

  // Number of times SIGALRM has been received.
  static int count() { return count_; }

 private:
  static void Handler(int sig) {
    CHECK_EQ(sig, SIGALRM);
    ++count_;
  }

  static void SetIntervalTimer(const int usec) {
    const itimerval tv = {{0, usec}, {0, usec}};
    if (setitimer(ITIMER_REAL, &tv, nullptr) < 0) {
      PLOG(FATAL) << "Cannot set timer";
    }
  }

  // Number of times SIGALRM has been received.
  static int count_;

  using SigHandler = void (*)(int);
  static SigHandler old_handler_;

  DISALLOW_COPY_AND_ASSIGN(AlarmGuard);
};

int AlarmGuard::count_ = 0;
AlarmGuard::SigHandler AlarmGuard::old_handler_ = nullptr;

// Anonymous pipe.
struct Pipe {
  base::ScopedFD read_fd, write_fd;

  // Creates an open pipe.
  Pipe() {
    int fds[2];
    if (pipe(fds) < 0) {
      PLOG(FATAL) << "Cannot create pipe ";
    }

    read_fd.reset(fds[0]);
    write_fd.reset(fds[1]);
  }
};

std::string Read(const base::ScopedFD fd) {
  char buffer[PIPE_BUF];

  LOG(INFO) << "Reading up to " << PIPE_BUF << " bytes from fd " << fd.get()
            << "...";
  const ssize_t bytes_read = HANDLE_EINTR(read(fd.get(), buffer, PIPE_BUF));
  PLOG_IF(FATAL, bytes_read < 0)
      << "Cannot read from file descriptor " << fd.get();

  LOG(INFO) << "Read " << bytes_read << " bytes from fd " << fd.get();
  return std::string(buffer, bytes_read);
}

void WriteAndClose(const base::ScopedFD fd, base::StringPiece s) {
  while (!s.empty()) {
    const ssize_t bytes_written =
        HANDLE_EINTR(write(fd.get(), s.data(), s.size()));
    PLOG_IF(FATAL, bytes_written < 0)
        << "Cannot write to file descriptor " << fd.get();

    s.remove_prefix(bytes_written);
  }
}

// A mock Process class for testing the Process base class.
class ProcessUnderTest : public Process {
 public:
  MOCK_METHOD(pid_t,
              StartImpl,
              (base::ScopedFD*, base::ScopedFD*, base::ScopedFD*),
              (override));
  MOCK_METHOD(int, WaitImpl, (), (override));
  MOCK_METHOD(bool, WaitNonBlockingImpl, (int*), (override));
};

struct ProcessFactory {
  base::StringPiece name;
  std::unique_ptr<Process> (*make_process)();
};

std::ostream& operator<<(std::ostream& out, const ProcessFactory& x) {
  return out << x.name;
}

}  // namespace

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

class ProcessRunTest : public ::testing::TestWithParam<ProcessFactory> {
 public:
  ProcessRunTest() {
    // Ensure that we get an error message if Minijail crashes.
    // TODO(crbug.com/1007098) Remove the following line or this comment
    // depending on how this bug is resolved.
    minijail_log_to_fd(STDERR_FILENO, 0);
  }

  const std::unique_ptr<Process> process_ = GetParam().make_process();
};

TEST_P(ProcessRunTest, ReturnsZero) {
  Process& process = *process_;
  process.AddArgument("/bin/true");
  EXPECT_EQ(process.Run(), 0);
}

TEST_P(ProcessRunTest, ReturnsNonZero) {
  Process& process = *process_;
  process.AddArgument("/bin/false");
  EXPECT_EQ(process.Run(), 1);
}

TEST_P(ProcessRunTest, KilledBySigKill) {
  Process& process = *process_;
  process.AddArgument("/bin/sh");
  process.AddArgument("-c");
  process.AddArgument("kill -KILL $$; sleep 1000");
  EXPECT_EQ(process.Run(), 128 + SIGKILL);
}

TEST_P(ProcessRunTest, KilledBySigSys) {
  Process& process = *process_;
  process.AddArgument("/bin/sh");
  process.AddArgument("-c");
  process.AddArgument("kill -SYS $$; sleep 1000");
  EXPECT_EQ(process.Run(), MINIJAIL_ERR_JAIL);
}

TEST_P(ProcessRunTest, CannotExec) {
  Process& process = *process_;
  process.AddArgument("non_existing_exe_foo_bar");
  // SandboxedProcess returns 255, but it isn't explicitly specified.
  EXPECT_GT(process.Run(), 0);
}

TEST_P(ProcessRunTest, CapturesInterleavedOutputs) {
  Process& process = *process_;
  process.AddArgument("/bin/sh");
  process.AddArgument("-c");
  process.AddArgument(R"(
      printf 'Line 1\nLine ' >&1;
      printf 'Line 2\nLine' >&2;
      printf '3\nLine 4\n' >&1;
      printf ' 5\nLine 6' >&2;
    )");

  std::vector<std::string> output;
  EXPECT_EQ(process.Run(&output), 0);
  EXPECT_THAT(output, UnorderedElementsAre("OUT: Line 1", "OUT: Line 3",
                                           "OUT: Line 4", "ERR: Line 2",
                                           "ERR: Line 5", "ERR: Line 6"));
}

TEST_P(ProcessRunTest, CapturesLotsOfOutputData) {
  Process& process = *process_;
  process.AddArgument("/bin/sh");
  process.AddArgument("-c");
  process.AddArgument(R"(
      for i in $(seq 1 1000); do
        printf 'Message %i\n' $i >&1;
        printf 'Error %i\n' $i >&2;
      done;
    )");

  std::vector<std::string> output;
  EXPECT_EQ(process.Run(&output), 0);
  EXPECT_THAT(output, SizeIs(2000));
}

TEST_P(ProcessRunTest, DoesNotBlockWhenReadingFromStdIn) {
  Process& process = *process_;
  process.AddArgument("/bin/cat");

  // By default, /bin/cat reads from stdin. If the pipe connected to stdin was
  // left open, the process would block indefinitely while reading from it.
  EXPECT_EQ(process.Run(), 0);
}

TEST_P(ProcessRunTest, DoesNotWaitForBackgroundProcessToFinish) {
  Process& process = *process_;
  process.AddArgument("/bin/sh");
  process.AddArgument("-c");

  // Pipe to unblock the background process and allow it to finish.
  Pipe p1;
  ASSERT_EQ(fcntl(p1.write_fd.get(), F_SETFD, FD_CLOEXEC), 0);
  // Pipe to monitor the background process and wait for it to finish.
  Pipe p2;
  ASSERT_EQ(fcntl(p2.read_fd.get(), F_SETFD, FD_CLOEXEC), 0);

  process.AddArgument(base::StringPrintf(R"(
      printf 'Begin\n';
      (
        exec 0<&-;
        exec 1>&-;
        exec 2>&-;
        read line <&%d;
        printf '%%s and End' "$line" >&%d;
        exit 42;
      )&
      printf 'Started background process %%i\n' $!
      exit 5;
    )",
                                         p1.read_fd.get(), p2.write_fd.get()));

  std::vector<std::string> output;
  EXPECT_EQ(process.Run(&output), 5);
  EXPECT_THAT(
      output,
      ElementsAre("OUT: Begin", StartsWith("OUT: Started background process")));

  // Unblock the orphaned background process.
  LOG(INFO) << "Unblocking background process";
  WriteAndClose(std::move(p1.write_fd), "Continue\n");

  // Wait for the orphaned background processes to finish. If the main test
  // program finishes before these background processes, the test framework
  // complains about leaked processes.
  p2.write_fd.reset();
  LOG(INFO) << "Waiting for background process to finish";
  EXPECT_EQ(Read(std::move(p2.read_fd)), "Continue and End");
}

TEST_P(ProcessRunTest, UndisturbedBySignalsWhenWaiting) {
  Process& process = *process_;
  process.AddArgument("/bin/sh");
  process.AddArgument("-c");
  process.AddArgument(R"(
      sleep 1;
      exit 42;
    )");

  // Activate an interval timer.
  const AlarmGuard guard(13 /* milliseconds */);
  EXPECT_TRUE(process.Start());
  EXPECT_EQ(process.Wait(), 42);
  EXPECT_GT(AlarmGuard::count(), 0);
}

INSTANTIATE_TEST_SUITE_P(ProcessRun,
                         ProcessRunTest,
                         Values(ProcessFactory{
                             "SandboxedProcess",
                             []() -> std::unique_ptr<Process> {
                               return std::make_unique<SandboxedProcess>();
                             }}),
                         PrintToStringParamName());

}  // namespace cros_disks
