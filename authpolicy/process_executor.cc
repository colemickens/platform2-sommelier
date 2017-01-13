// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "authpolicy/process_executor.h"

#include <stdlib.h>
#include <utility>

#include <base/files/file_path.h>
#include <base/files/scoped_file.h>
#include <base/strings/stringprintf.h>
#include <libminijail.h>

#include "authpolicy/pipe_helper.h"

namespace ah = authpolicy::helper;

namespace authpolicy {

ProcessExecutor::ProcessExecutor(std::vector<std::string> args)
    : jail_(minijail_new()), args_(std::move(args)) {}

ProcessExecutor::~ProcessExecutor() {
  minijail_destroy(jail_);
}

void ProcessExecutor::SetInputFile(int fd) {
  input_fd_ = fd;
}

void ProcessExecutor::SetInputString(const std::string& input_str) {
  input_str_ = input_str;
}

void ProcessExecutor::SetEnv(const std::string& key, const std::string& value) {
  env_map_[key] = value;
}

void ProcessExecutor::SetSeccompFilter(const std::string& policy_file) {
  minijail_parse_seccomp_filters(jail_, policy_file.c_str());
  minijail_use_seccomp_filter(jail_);
}

void ProcessExecutor::LogSeccompFilterFailures() {
  minijail_log_seccomp_filter_failures(jail_);
}

void ProcessExecutor::SetNoNewPrivs() {
  minijail_no_new_privs(jail_);
}

void ProcessExecutor::ChangeUser(const char* user) {
  minijail_change_user(jail_, user);
}

void ProcessExecutor::KeepSupplementaryGroups() {
  minijail_keep_supplementary_gids(jail_);
}

bool ProcessExecutor::Execute() {
  ResetOutput();
  if (args_.empty() || args_[0].empty()) return true;

  if (!base::FilePath(args_[0]).IsAbsolute()) {
    LOG(ERROR) << "Command must be specified by absolute path.";
    exit_code_ = kExitCodeInternalError;
    return false;
  }

  if (LOG_IS_ON(INFO)) {
    std::string cmd = args_[0];
    for (size_t n = 1; n < args_.size(); ++n)
      cmd += base::StringPrintf(" '%s'", args_[n].c_str());
    LOG(INFO) << "Executing " << cmd;
  }

  // Convert args to array of pointers. Must be nullptr terminated.
  std::vector<char*> args_ptr;
  for (const auto& arg : args_)
    args_ptr.push_back(const_cast<char*>(arg.c_str()));
  args_ptr.push_back(nullptr);

  // Save old environment and set ours. Note that clearenv() doesn't actually
  // delete any pointers, so we can just keep the old pointers.
  std::vector<char*> old_environ;
  for (char** env = environ; env != nullptr && *env != nullptr; ++env)
    old_environ.push_back(*env);
  clearenv();
  std::vector<std::string> env_list;
  for (const auto& env : env_map_) {
    env_list.push_back(env.first + "=" + env.second);
    putenv(const_cast<char*>(env_list.back().c_str()));
  }

  // Execute the command.
  pid_t pid = -1;
  int child_stdin = -1, child_stdout = -1, child_stderr = -1;
  minijail_run_pid_pipes_no_preload(jail_, args_ptr[0], args_ptr.data(), &pid,
                                    &child_stdin, &child_stdout, &child_stderr);

  // Restore the environment.
  clearenv();
  for (char* env : old_environ)
    putenv(env);

  // Make sure pipes get closed when exiting the scope.
  base::ScopedFD child_stdin_closer(child_stdin);
  base::ScopedFD child_stdout_closer(child_stdout);
  base::ScopedFD child_stderr_closer(child_stderr);

  // Write input to stdin. On error, do NOT return before minijail_wait or else
  // the process will leak!
  bool write_to_stdin_failed = false;
  if (input_fd_ != -1 && !ah::CopyPipe(input_fd_, child_stdin_closer.get())) {
    LOG(ERROR) << "Failed to copy input pipe to child stdin";
    write_to_stdin_failed = true;
  } else if (!input_str_.empty() &&
             !ah::WriteStringToPipe(input_str_, child_stdin_closer.get())) {
    LOG(ERROR) << "Failed to write input string to child stdin";
    write_to_stdin_failed = true;
  }
  child_stdin_closer.reset();

  // Wait for the process to exit.
  exit_code_ = minijail_wait(jail_);

  // Print out a useful error message for seccomp failures.
  if (exit_code_ == MINIJAIL_ERR_JAIL)
    LOG(ERROR) << "Seccomp filter blocked a system call";

  // Always exit AFTER minijail_wait!
  if (write_to_stdin_failed) {
    exit_code_ = kExitCodeInternalError;
    return false;
  }

  // Read output.
  if (!ah::ReadPipeToString(child_stdout_closer.get(), &out_data_)) {
    LOG(ERROR) << "Failed to read data from child stdout";
    exit_code_ = kExitCodeInternalError;
    return false;
  }
  if (!ah::ReadPipeToString(child_stderr_closer.get(), &err_data_)) {
    LOG(ERROR) << "Failed to read data from child stderr";
    exit_code_ = kExitCodeInternalError;
    return false;
  }

  LOG(INFO) << "Stdout: " << out_data_;
  LOG(INFO) << "Stderr: " << err_data_;
  LOG(INFO) << "Exit code: " << exit_code_;
  return exit_code_ == 0;
}

void ProcessExecutor::ResetOutput() {
  exit_code_ = 0;
  out_data_.clear();
  err_data_.clear();
}

}  // namespace authpolicy
