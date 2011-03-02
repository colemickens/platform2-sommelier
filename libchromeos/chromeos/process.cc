// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <chromeos/process.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>

#include <map>

#include <base/eintr_wrapper.h>
#include <base/file_util.h>
#include <base/logging.h>
#include <base/string_number_conversions.h>
#include <base/string_util.h>
#include <base/time.h>

namespace chromeos {

Process::Process() {
}

Process::~Process() {
}

bool Process::ProcessExists(pid_t pid) {
  return file_util::DirectoryExists(FilePath(StringPrintf("/proc/%d", pid)));
}

ProcessImpl::ProcessImpl() : pid_(0) {
}

ProcessImpl::~ProcessImpl() {
  Reset(0);
}

void ProcessImpl::AddArg(const std::string& arg) {
  arguments_.push_back(arg);
}

void ProcessImpl::RedirectOutput(const std::string& output_file) {
  output_file_ = output_file;
}

bool ProcessImpl::Start() {
  // Copy off a writeable version of arguments.  Exec wants to be able
  // to write both the array and the strings.
  int total_args_size = 0;

  for (size_t i = 0; i < arguments_.size(); ++i) {
    total_args_size += arguments_[i].size() + 1;
  }

  scoped_array<char> buffer(new char[total_args_size]);
  scoped_array<char*> argv(new char*[arguments_.size() + 1]);

  char* buffer_pointer = &buffer[0];
  for (size_t i = 0; i < arguments_.size(); ++i) {
    argv[i] = buffer_pointer;
    strcpy(buffer_pointer, arguments_[i].c_str());
    buffer_pointer += arguments_[i].size() + 1;
  }
  argv[arguments_.size()] = NULL;

  pid_t pid = fork();
  int saved_errno = errno;
  if (pid  < 0) {
    LOG(ERROR) << "Fork failed: " << saved_errno;
    return false;
  }

  if (pid == 0) {
    if (!output_file_.empty()) {
      int output_handle = HANDLE_EINTR(
          open(output_file_.c_str(),
               O_CREAT | O_WRONLY | O_TRUNC | O_EXCL, 0666));
      int saved_errno = errno;
      if (output_handle < 0) {
        LOG(ERROR) << "Could not create " << output_file_
                   << ": " << saved_errno;
        // Avoid exit() to avoid atexit handlers from parent.
        _exit(127);
      }
      dup2(output_handle, STDOUT_FILENO);
      dup2(output_handle, STDERR_FILENO);
      // Only close output_handle if it does not happen to be one of
      // the two standard file descriptors we are trying to redirect.
      if (output_handle != STDOUT_FILENO && output_handle != STDERR_FILENO)
        close(output_handle);
    }
    execv(argv[0], &argv[0]);
    saved_errno = errno;
    LOG(ERROR) << "Exec of " << argv[0] << " failed: " << saved_errno;
    _exit(127);
  } else {
    Reset(pid);
  }
  return true;
}

int ProcessImpl::Wait() {
  int status = 0;
  if (pid_ == 0) {
    LOG(ERROR) << "Process not running";
    return -1;
  }
  if (HANDLE_EINTR(waitpid(pid_, &status, 0)) < 0) {
    int saved_errno = errno;
    LOG(ERROR) << "Problem waiting for pid " << pid_ << ": " << saved_errno;
    return -1;
  }
  pid_t old_pid = pid_;
  // Update the pid to 0 - do not Reset as we do not want to try to
  // kill the process that has just exited.
  UpdatePid(0);
  if (!WIFEXITED(status)) {
    LOG(ERROR) << "Process " << old_pid << " did not exit normally: "
               << status;
    return -1;
  }
  return WEXITSTATUS(status);
}

int ProcessImpl::Run() {
  if (!Start()) {
    return -1;
  }
  return Wait();
}

pid_t ProcessImpl::pid() {
  return pid_;
}

bool ProcessImpl::Kill(int signal, int timeout) {
  if (pid_ == 0) {
    // Passing pid == 0 to kill is committing suicide.  Check specifically.
    LOG(ERROR) << "Process not running";
    return false;
  }
  if (kill(pid_, signal) < 0) {
    int saved_errno = errno;
    LOG(ERROR) << "Unable to send signal to " << pid_ << " error "
               << saved_errno;
    return false;
  }
  base::TimeTicks start_signal = base::TimeTicks::Now();
  do {
    int status = 0;
    pid_t w = waitpid(pid_, &status, WNOHANG);
    int saved_errno = errno;
    if (w < 0) {
      if (saved_errno == ECHILD)
        return true;
      LOG(ERROR) << "Waitpid returned " << w << ", errno " << saved_errno;
      return false;
    }
    if (w > 0) {
      Reset(0);
      return true;
    }
    usleep(100);
  } while ((base::TimeTicks::Now() - start_signal).InSecondsF() <= timeout);
  LOG(INFO) << "process " << pid_ << " did not exit from signal " << signal
            << " in " << timeout << " seconds";
  return false;
}

void ProcessImpl::UpdatePid(pid_t new_pid) {
  pid_ = new_pid;
}

void ProcessImpl::Reset(pid_t new_pid) {
  arguments_.clear();
  if (pid_)
    Kill(SIGKILL, 0);
  UpdatePid(new_pid);
}

bool ProcessImpl::ResetPidByFile(const std::string& pid_file) {
  std::string contents;
  if (!file_util::ReadFileToString(FilePath(pid_file), &contents)) {
    LOG(ERROR) << "Could not read pid file" << pid_file;
    return false;
  }
  TrimWhitespaceASCII(contents, TRIM_TRAILING, &contents);
  int64 pid_int64 = 0;
  if (!base::StringToInt64(contents, &pid_int64)) {
    LOG(ERROR) << "Unexpected pid file contents";
    return false;
  }
  Reset(pid_int64);
  return true;
}

pid_t ProcessImpl::Release() {
  pid_t old_pid = pid_;
  pid_ = 0;
  return old_pid;
}

}  // namespace chromeos
