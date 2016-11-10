// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef AUTHPOLICY_PROCESS_EXECUTOR_H_
#define AUTHPOLICY_PROCESS_EXECUTOR_H_

#include <map>
#include <string>
#include <utility>
#include <vector>

namespace authpolicy {

// Helper class to execute commands and piping data
class ProcessExecutor {
 public:
  static ProcessExecutor Create(std::vector<std::string> args) {
    return ProcessExecutor(std::move(args));
  }

  explicit ProcessExecutor(std::vector<std::string> args)
      : args_(std::move(args)) { }

  // Set a file descriptor that gets piped into stdin during execution.
  // The file descriptor must stay valid until |Execute| is called.
  ProcessExecutor& SetInputFile(int fd);

  // Set an environment variable '|key|=|value|, which is passed into the
  // process to be executed. Any number of variables can be set.
  ProcessExecutor& SetEnv(const std::string& key, const std::string& value);

  // Execute the command. Returns true if the command executed and returned with
  // exit code 0. Also returns true if no args were passed to the constructor.
  // Returns false otherwise.
  // Calling |Execute| multiple times is possible. Note, however, that you might
  // have to call |SetInputFile| again if the input pipe was fully read.
  // Getters should only be called after execution.
  bool Execute();

  // Populated after execute call.
  const std::string& GetStdout() const { return out_data_; }
  const std::string& GetStderr() const { return err_data_; }
  const int GetExitCode() const { return exit_code_; }

 private:
  // Resets the output variables that are populated by |Execute|.
  void ResetOutput();

  std::vector<std::string> args_;
  std::map<std::string, std::string> env_;
  int input_fd_ = -1;
  std::string out_data_, err_data_;
  int exit_code_ = 0;
};

}  // namespace authpolicy

#endif  // AUTHPOLICY_PROCESS_EXECUTOR_H_
