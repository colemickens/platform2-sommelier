// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PROCESS_H_
#define CHROMEOS_PROCESS_H_

#include <sys/types.h>

#include <string>
#include <vector>

#include <base/string_util.h>

namespace chromeos {
// Manages a process.  Can create the process, attach to an existing
// process by pid or pid file, and kill the process.  Upon destruction
// any managed process is killed with SIGKILL.  Use Release() to
// release the process from management.  A given system process may
// only be managed by one Process at a time.
class Process {
 public:
  Process();
  virtual ~Process();

  // Adds |arg| to the executable command-line to be run.  The
  // executable name itself is the first argument.
  virtual void AddArg(const std::string& arg) = 0;

  // Adds |option| and |value| as an option with a string value to the
  // command line to be run.
  inline void AddStringOption(const std::string& option,
                              const std::string& value) {
    AddArg(option);
    AddArg(value);
  }

  // Adds |option| and |value| as an option which takes an integer
  // value to the command line to be run.
  inline void AddIntOption(const std::string& option, int value) {
    AddArg(option);
    AddArg(StringPrintf("%d", value));
  }

  // Redirects stderr and stdout to |output_file|.
  virtual void RedirectOutput(const std::string& output_file) = 0;

  // Starts this process, returning true if successful.
  virtual bool Start() = 0;

  // Waits for this process to finish.  Returns the process's exit
  // status if it exited normally, or otherwise returns -1.
  virtual int Wait() = 0;

  // Start and wait for this process to finish.  Returns same value as
  // Wait().
  virtual int Run() = 0;

  // Returns the pid of this process or else returns 0 if there is no
  // corresponding process (either because it has not yet been started
  // or has since exited).
  virtual pid_t pid() = 0;

  // Kills this process with |signal|.  If process is not a child,
  // returns immediately with a value based on whether kill was
  // successful.  If the process is a child and |timeout| is non-zero,
  // returns true if the process is able to be reaped within the given
  // |timeout| in seconds.
  virtual bool Kill(int signal, int timeout) = 0;

  // Resets this Process object to refer to the process with |pid|.
  // If |pid| is zero, this object no longer refers to a process.
  virtual void Reset(pid_t new_pid) = 0;

  // Same as Reset but reads the pid from |pid_file|.  Returns false
  // only when the file cannot be read/parsed.
  virtual bool ResetPidByFile(const std::string& pid_file) = 0;

  // Releases the process so that on destruction, the process is not killed.
  virtual pid_t Release() = 0;

  // Returns if |pid| is a currently running process.
  static bool ProcessExists(pid_t pid);
};

class ProcessImpl : public Process {
 public:
  ProcessImpl();
  virtual ~ProcessImpl();

  virtual void AddArg(const std::string& arg);
  virtual void RedirectOutput(const std::string& output_file);
  virtual bool Start();
  virtual int Wait();
  virtual int Run();
  virtual pid_t pid();
  virtual bool Kill(int signal, int timeout);
  virtual void Reset(pid_t pid);
  virtual bool ResetPidByFile(const std::string& pid_file);
  virtual pid_t Release();

 protected:
  std::string output_file_;
  std::vector<std::string> arguments_;

  void UpdatePid(pid_t new_pid);

 private:
  // Pid of currently managed process or 0 if no currently managed
  // process.  pid must not be modified except by calling
  // UpdatePid(new_pid).
  pid_t pid_;
};

}  // namespace chromeos

#endif  // CHROMEOS_PROCESS_H
