// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "authpolicy/process_executor.h"

#include <algorithm>

#include "base/files/file_util.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"

namespace authpolicy {

namespace {

// The maximum number of bytes to get from a child process
// (from stdout and stderr).
const size_t kMaxReadSize = 100*1024;  // 100 Kb
// The size of the buffer used to fetch stdout and stderr.
const size_t kBufferSize = 1024;  // 1 Kb
// The maximum timeout on waiting a command to run.
const base::TimeDelta kMaxTimeout = base::TimeDelta::FromSeconds(30);

void ReadPipe(int fd, std::string* out) {
  char buffer[kBufferSize];
  size_t total_read = 0;
  while (total_read < kMaxReadSize) {
    const ssize_t bytes_read =
        HANDLE_EINTR(read(fd, buffer,
                          std::min(kBufferSize, kMaxReadSize - total_read)));
    if (bytes_read < 0)
      PLOG(ERROR) << "Failed to read from pipe.";
    if (bytes_read <= 0)
      break;
    total_read += bytes_read;
    out->append(buffer, bytes_read);
  }
}

}  // namespace

ProcessExecutor& ProcessExecutor::SetInputFile(int fd) {
  fds_to_remap_.push_back(std::make_pair(fd, STDIN_FILENO));
  return *this;
}

ProcessExecutor& ProcessExecutor::SetEnv(
    const std::map<std::string, std::string>& environ) {
  launch_options_.environ = environ;
  return *this;
}

bool ProcessExecutor::Execute() {
  int stdout_pipe[2];
  if (!base::CreateLocalNonBlockingPipe(stdout_pipe)) {
    LOG(ERROR) << "Failed to create pipe.";
    return false;
  }
  base::ScopedFD stdout_read_end(stdout_pipe[0]);
  base::ScopedFD stdout_write_end(stdout_pipe[1]);

  int stderr_pipe[2];
  if (!base::CreateLocalNonBlockingPipe(stderr_pipe)) {
    LOG(ERROR) << "Failed to create pipe.";
    return false;
  }
  base::ScopedFD stderr_read_end(stderr_pipe[0]);
  base::ScopedFD stderr_write_end(stderr_pipe[1]);

  fds_to_remap_.push_back(std::make_pair(stdout_write_end.get(),
                                         STDOUT_FILENO));
  fds_to_remap_.push_back(std::make_pair(stderr_write_end.get(),
                                         STDERR_FILENO));
  launch_options_.fds_to_remap = &fds_to_remap_;
  launch_options_.clear_environ = true;
  if (LOG_IS_ON(INFO)) {
    if (!args_.empty()) {
      std::string cmd = args_[0];
      for (size_t n = 1; n < args_.size(); ++n)
        cmd += base::StringPrintf(" '%s'", args_[n].c_str());
      LOG(INFO) << "Executing " << cmd;
    }
  }

  base::Process process(base::LaunchProcess(args_, launch_options_));
  if (!process.IsValid()) {
    LOG(ERROR) << "Failed to create process";
    return false;
  }
  stdout_write_end.reset();
  stderr_write_end.reset();

  if (!process.WaitForExitWithTimeout(kMaxTimeout, &exit_code_)) {
    LOG(ERROR) << "Failed on waiting the process to stop";
    return false;
  }

  ReadPipe(stdout_read_end.get(), &out_data_);
  ReadPipe(stderr_read_end.get(), &err_data_);

  LOG(INFO) << "Stdout: " << GetStdout();
  LOG(INFO) << "Stderr: " << GetStderr();
  LOG(INFO) << "Exit code: " << GetExitCode();
  return GetExitCode() == 0;
}

}  // namespace authpolicy
