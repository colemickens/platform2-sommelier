// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "vm_tools/garcon/ansible_playbook_application.h"

#include <errno.h>
#include <fcntl.h>
#include <map>
#include <sstream>
#include <unistd.h>
#include <utility>
#include <vector>

#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/files/scoped_file.h>
#include <base/posix/safe_strerror.h>
#include <base/synchronization/waitable_event.h>

#include "vm_tools/common/spawn_util.h"

namespace vm_tools {
namespace garcon {
namespace {

constexpr char kStdoutCallbackEnv[] = "ANSIBLE_STDOUT_CALLBACK";
constexpr char kDefaultCallbackPluginPathEnv[] = "ANSIBLE_CALLBACK_PLUGINS";
constexpr char kStdoutCallbackName[] = "garcon";
constexpr char kDefaultCallbackPluginPath[] =
    "/usr/share/ansible/plugins/callback";

// Return true on successful ansible-playbook result and false otherwise.
bool GetPlaybookApplicationResult(const std::string& stdout,
                                  const std::string& stderr,
                                  std::string* failure_reason) {
  const std::string execution_info =
      "Ansible playbook application stdout:\n" + stdout + "\n" +
      "Ansible playbook application stderr:\n" + stderr + "\n";

  if (stdout.find("MESSAGE TO GARCON: TASK_FAILED") != std::string::npos) {
    LOG(INFO) << "Some tasks failed during container configuration";
    *failure_reason = execution_info;
    return false;
  }
  if (!stderr.empty()) {
    *failure_reason = execution_info;
    return false;
  }
  return true;
}

bool CreatePipe(base::ScopedFD* read_fd,
                base::ScopedFD* write_fd,
                std::string* error_msg) {
  int fds[2];
  if (pipe2(fds, O_CLOEXEC) < 0) {
    *error_msg =
        "Failed to open target process pipe: " + base::safe_strerror(errno);
    return false;
  }
  read_fd->reset(fds[0]);
  write_fd->reset(fds[1]);
  return true;
}

}  // namespace

void ExecuteAnsiblePlaybook(AnsiblePlaybookApplicationObserver* observer,
                            base::WaitableEvent* event,
                            const base::FilePath& ansible_playbook_file_path,
                            std::string* error_msg) {
  std::vector<std::string> argv{"ansible-playbook",
                                "--become",
                                "--connection=local",
                                "--inventory",
                                "127.0.0.1,",
                                ansible_playbook_file_path.value(),
                                "-e",
                                "ansible_python_interpreter=/usr/bin/python3"};

  std::map<std::string, std::string> env;
  env[kStdoutCallbackEnv] = kStdoutCallbackName;
  env[kDefaultCallbackPluginPathEnv] = kDefaultCallbackPluginPath;

  // Set child's process stdout and stderr to write end of pipes.
  int stdio_fd[] = {-1, -1, -1};

  base::ScopedFD read_stdout;
  base::ScopedFD write_stdout;
  if (!CreatePipe(&read_stdout, &write_stdout, error_msg)) {
    return;
  }

  base::ScopedFD read_stderr;
  base::ScopedFD write_stderr;
  if (!CreatePipe(&read_stderr, &write_stderr, error_msg)) {
    return;
  }

  stdio_fd[STDOUT_FILENO] = write_stdout.get();
  stdio_fd[STDERR_FILENO] = write_stderr.get();

  if (!Spawn(std::move(argv), std::move(env), "", stdio_fd)) {
    *error_msg = "Failed to spawn ansible-playbook process";
    return;
  }

  write_stdout.reset();
  write_stderr.reset();
  event->Signal();

  // TODO(okalitova): Add file watchers.
  char buffer[100];
  std::stringstream stdout;
  ssize_t count = read(read_stdout.get(), buffer, sizeof(buffer));
  while (count > 0) {
    stdout.write(buffer, count);
    count = read(read_stdout.get(), buffer, sizeof(buffer));
  }
  std::stringstream stderr;
  count = read(read_stderr.get(), buffer, sizeof(buffer));
  while (count > 0) {
    stderr.write(buffer, count);
    count = read(read_stderr.get(), buffer, sizeof(buffer));
  }

  std::string failure_reason;
  bool success =
      GetPlaybookApplicationResult(stdout.str(), stderr.str(), &failure_reason);

  observer->OnApplyAnsiblePlaybookCompletion(success, failure_reason);
  return;
}

base::FilePath CreateAnsiblePlaybookFile(const std::string& playbook,
                                         std::string* error_msg) {
  base::FilePath ansible_dir;
  bool success = base::CreateNewTempDirectory("", &ansible_dir);
  if (!success) {
    LOG(ERROR) << "Failed to create directory for ansible playbook file";
    *error_msg = "Failed to create directory for ansible playbook file";
    return base::FilePath();
  }

  const base::FilePath ansible_playbook_file_path =
      ansible_dir.Append("playbook.yaml");
  base::File ansible_playbook_file(
      ansible_playbook_file_path,
      base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);

  if (!ansible_playbook_file.created()) {
    *error_msg = "Failed to create file for Ansible playbook";
    return base::FilePath();
  }
  if (!ansible_playbook_file.IsValid()) {
    *error_msg = "Failed to create valid file for Ansible playbook";
    return base::FilePath();
  }

  int bytes = ansible_playbook_file.WriteAtCurrentPos(playbook.c_str(),
                                                      playbook.length());

  if (bytes != playbook.length()) {
    *error_msg = "Failed to write Ansible playbook content to file";
    return base::FilePath();
  }

  return ansible_playbook_file_path;
}

}  // namespace garcon
}  // namespace vm_tools
