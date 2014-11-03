// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "debugd/src/process_with_output.h"

#include <signal.h>

#include <base/files/file_util.h>
#include <base/strings/string_split.h>

namespace debugd {

namespace {

const char kDBusErrorString[] = "org.chromium.debugd.error.RunProcess";
const char kInitErrorString[] = "Process initialization failure.";
const char kStartErrorString[] = "Process start failure.";
const char kInputErrorString[] = "Process input write failure.";
const char kPathLengthErrorString[] = "Path length is too long.";

// Sets the D-Bus error if it's non-NULL.
void SetError(const char* message, DBus::Error* error) {
  if (error) {
    error->set(kDBusErrorString, message);
  }
}

}  // namespace

ProcessWithOutput::ProcessWithOutput()
    : separate_stderr_(false), use_minijail_(true) {
}

ProcessWithOutput::~ProcessWithOutput() {
  outfile_.reset();
  errfile_.reset();

  if (!outfile_path_.empty())
    base::DeleteFile(outfile_path_, false);  // not recursive
  if (!errfile_path_.empty())
    base::DeleteFile(errfile_path_, false);
}

bool ProcessWithOutput::Init() {
  if (use_minijail_) {
    if (!SandboxedProcess::Init())
      return false;
  }

  outfile_.reset(base::CreateAndOpenTemporaryFile(&outfile_path_));
  if (!outfile_.get()) {
    return false;
  }
  if (separate_stderr_) {
    errfile_.reset(base::CreateAndOpenTemporaryFile(&errfile_path_));
    if (!errfile_.get()) {
      return false;
    }
  }

  // We can't just RedirectOutput to the file we just created, since
  // RedirectOutput uses O_CREAT | O_EXCL to open the target file (i.e., it'll
  // fail if the file already exists). We can't CreateTemporaryFile() and then
  // use that filename, since we'd have to remove it before using
  // RedirectOutput, which exposes us to a /tmp race. Instead, bind outfile_'s
  // fd to the subprocess's stdout and stderr.
  BindFd(fileno(outfile_.get()), STDOUT_FILENO);
  BindFd(fileno(separate_stderr_ ? errfile_.get() : outfile_.get()),
         STDERR_FILENO);
  return true;
}

bool ProcessWithOutput::GetOutputLines(std::vector<std::string>* output) {
  std::string contents;
  if (!base::ReadFileToString(outfile_path_, &contents))
    return false;

  base::SplitString(contents, '\n', output);
  return true;
}

bool ProcessWithOutput::GetOutput(std::string* output) {
  return base::ReadFileToString(outfile_path_, output);
}

bool ProcessWithOutput::GetError(std::string* error) {
  return base::ReadFileToString(errfile_path_, error);
}

int ProcessWithOutput::RunProcess(const std::string& command,
                                  const ArgList& arguments,
                                  bool requires_root,
                                  const std::string* stdin,
                                  std::string* stdout,
                                  std::string* stderr,
                                  DBus::Error* error) {
  ProcessWithOutput process;
  if (requires_root) {
    process.SandboxAs("root", "root");
  }
  return DoRunProcess(
      command, arguments, stdin, stdout, stderr, error, &process);
}

int ProcessWithOutput::RunHelper(const std::string& helper,
                                 const ArgList& arguments,
                                 bool requires_root,
                                 const std::string* stdin,
                                 std::string* stdout,
                                 std::string* stderr,
                                 DBus::Error* error) {
  std::string helper_path;
  if (!SandboxedProcess::GetHelperPath(helper, &helper_path)) {
    SetError(kPathLengthErrorString, error);
    return kRunError;
  }
  return RunProcess(
      helper_path, arguments, requires_root, stdin, stdout, stderr, error);
}

int ProcessWithOutput::RunProcessFromHelper(const std::string& command,
                                            const ArgList& arguments,
                                            const std::string* stdin,
                                            std::string* stdout,
                                            std::string* stderr) {
  ProcessWithOutput process;
  process.set_use_minijail(false);
  process.SetSearchPath(true);
  return DoRunProcess(
      command, arguments, stdin, stdout, stderr, nullptr, &process);
}

int ProcessWithOutput::DoRunProcess(const std::string& command,
                                    const ArgList& arguments,
                                    const std::string* stdin,
                                    std::string* stdout,
                                    std::string* stderr,
                                    DBus::Error* error,
                                    ProcessWithOutput* process) {
  process->set_separate_stderr(true);
  if (!process->Init()) {
    SetError(kInitErrorString, error);
    return kRunError;
  }

  process->AddArg(command);
  for (const auto& argument : arguments) {
    process->AddArg(argument);
  }

  int result = kRunError;
  if (stdin) {
    process->RedirectUsingPipe(STDIN_FILENO, true);
    if (process->Start()) {
      int stdin_fd = process->GetPipe(STDIN_FILENO);
      // Kill the process if writing to or closing the pipe fails.
      if (base::WriteFileDescriptor(stdin_fd, stdin->c_str(),
                                    stdin->length()) < 0 ||
          IGNORE_EINTR(close(stdin_fd)) < 0) {
        process->Kill(SIGKILL, 0);
        SetError(kInputErrorString, error);
      }
      result = process->Wait();
    } else {
      SetError(kStartErrorString, error);
    }
  } else {
    result = process->Run();
  }

  if (stdout) {
    process->GetOutput(stdout);
  }
  if (stderr) {
    process->GetError(stderr);
  }
  return result;
}

}  // namespace debugd
