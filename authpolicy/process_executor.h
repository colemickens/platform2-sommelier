// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef AUTHPOLICY_PROCESS_EXECUTOR_H_
#define AUTHPOLICY_PROCESS_EXECUTOR_H_

#include <map>
#include <string>
#include <vector>

#include <base/macros.h>

struct minijail;

namespace authpolicy {

// Helper class to execute commands and piping data. Uses minijail.
class ProcessExecutor {
 public:
  explicit ProcessExecutor(std::vector<std::string> args);
  ~ProcessExecutor();

  // Set a file descriptor that gets piped into stdin during execution.
  // The file descriptor must stay valid until |Execute| is called.
  void SetInputFile(int fd);

  // Set a string that gets written to stdin during execution. If a file
  // descriptor is set as well, this string is appended to its data.
  void SetInputString(const std::string& input_str);

  // Set an environment variable '|key|=|value|, which is passed into the
  // process to be executed. Any number of variables can be set.
  void SetEnv(const std::string& key, const std::string& value);

  // Sets a seccomp filter by parsing the given file.
  void SetSeccompFilter(const std::string& policy_file);

  // Set a flag that prevents execve from gaining new privileges.
  void SetNoNewPrivs();

  // Execute command as |user|.
  void ChangeUser(const char* user);

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

  // GetExitCode() returns this if some internal error in Execute() occurred,
  // e.g. failed to copy stdin pipes. Not an actual return code from execve.
  const int kExitCodeInternalError = 127;

 private:
  // Resets the output variables that are populated by |Execute|.
  void ResetOutput();

  minijail* jail_ = nullptr;
  std::vector<std::string> args_;
  std::map<std::string, std::string> env_map_;
  int input_fd_ = -1;
  std::string input_str_;
  std::string out_data_, err_data_;
  int exit_code_ = 0;

  // We better not copy/assign because of jail_.
  DISALLOW_COPY_AND_ASSIGN(ProcessExecutor);
};

}  // namespace authpolicy

#endif  // AUTHPOLICY_PROCESS_EXECUTOR_H_
