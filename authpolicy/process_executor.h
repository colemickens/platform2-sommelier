// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef AUTHPOLICY_PROCESS_EXECUTOR_H_
#define AUTHPOLICY_PROCESS_EXECUTOR_H_

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "base/process/launch.h"

namespace authpolicy {

// Helper class to execute commands and piping data
class ProcessExecutor {
 public:
  static ProcessExecutor Create(std::vector<std::string> args) {
    return ProcessExecutor(std::move(args));
  }

  explicit ProcessExecutor(std::vector<std::string> args)
      : args_(std::move(args)), exit_code_(0) {}

  // Set a file descriptor that gets piped into stdin during execution.
  // The file descriptor must stay valid until Execute called.
  ProcessExecutor& SetInputFile(int fd);

  // Set environment variables, a vector of strings 'variable=value', which are
  // passed into the process to be executed.
  ProcessExecutor& SetEnv(const std::map<std::string, std::string>& environ);

  // Execute the command. Returns true if command executed and returned with
  // exit code 0. Returns false if some error occurred.
  // Const methods only should be called after that.
  bool Execute();

  // Those are populated after execute call.
  const std::string& GetStdout() const { return out_data_; }
  const std::string& GetStderr() const { return err_data_; }
  const int GetExitCode() const { return exit_code_; }

 private:
  std::vector<std::string> args_;
  base::LaunchOptions launch_options_;
  base::FileHandleMappingVector fds_to_remap_;
  std::string out_data_, err_data_;
  int exit_code_;
};
}  // namespace authpolicy
#endif  // AUTHPOLICY_PROCESS_EXECUTOR_H_
