//
// Copyright (C) 2015 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#include "shill/process_manager.h"

#include <signal.h>
#include <stdlib.h>
#include <sys/prctl.h>

#include "shill/event_dispatcher.h"

using base::Closure;
using std::map;
using std::string;
using std::vector;

namespace shill {

namespace {

base::LazyInstance<ProcessManager> g_process_manager =
    LAZY_INSTANCE_INITIALIZER;

static const int kTerminationTimeoutSeconds = 2;

bool SetupChild(const map<string, string>& env, bool terminate_with_parent) {
  // Setup environment variables.
  clearenv();
  for (const auto& key_value : env) {
    setenv(key_value.first.c_str(), key_value.second.c_str(), 0);
  }
  if (terminate_with_parent) {
    prctl(PR_SET_PDEATHSIG, SIGTERM);
  }
  return true;
}

}  // namespace

ProcessManager::ProcessManager() {}

ProcessManager::~ProcessManager() {}

// static
ProcessManager* ProcessManager::GetInstance() {
  return g_process_manager.Pointer();
}

void ProcessManager::Init(EventDispatcher* dispatcher) {
  CHECK(!async_signal_handler_);
  async_signal_handler_.reset(new chromeos::AsynchronousSignalHandler());
  async_signal_handler_->Init();
  process_reaper_.Register(async_signal_handler_.get());
  dispatcher_ = dispatcher;
  minijail_ = chromeos::Minijail::GetInstance();
}

void ProcessManager::Stop() {
  CHECK(async_signal_handler_);
  process_reaper_.Unregister();
  async_signal_handler_.reset();
}

pid_t ProcessManager::StartProcess(
    const tracked_objects::Location& spawn_source,
    const base::FilePath& program,
    const vector<string>& arguments,
    const map<string, string>& environment,
    bool terminate_with_parent,
    const base::Callback<void(int)>& exit_callback) {
  // Setup/create child process.
  std::unique_ptr<chromeos::Process> process(new chromeos::ProcessImpl());
  process->AddArg(program.value());
  for (const auto& option : arguments) {
    process->AddArg(option);
  }
  process->SetPreExecCallback(
      base::Bind(&SetupChild, environment, terminate_with_parent));
  if (!process->Start()) {
    LOG(ERROR) << "Failed to start child process for " << program.value();
    return -1;
  }

  // Setup watcher for the child process.
  pid_t pid = process->pid();
  CHECK(process_reaper_.WatchForChild(
      spawn_source,
      pid,
      base::Bind(&ProcessManager::OnProcessExited,
                 weak_factory_.GetWeakPtr(),
                 pid)));

  // Release ownership of the child process from the |process| object, so that
  // child process will not get killed on destruction of |process| object.
  process->Release();

  watched_processes_.emplace(pid, exit_callback);
  return pid;
}

pid_t ProcessManager::StartProcessInMinijailWithPipes(
    const tracked_objects::Location& spawn_source,
    const base::FilePath& program,
    const std::vector<std::string>& arguments,
    const std::string& user,
    const std::string& group,
    uint64_t capmask,
    const base::Callback<void(int)>& exit_callback,
    int* stdin_fd,
    int* stdout_fd,
    int* stderr_fd) {
  vector<char*> args;
  args.push_back(const_cast<char*>(program.value().c_str()));
  for (const auto& arg : arguments) {
    args.push_back(const_cast<char*>(arg.c_str()));
  }
  args.push_back(nullptr);

  struct minijail* jail = minijail_->New();
  if (!minijail_->DropRoot(jail, user.c_str(), group.c_str())) {
    LOG(ERROR) << "Minijail failed to drop root privileges?";
    return -1;
  }
  minijail_->UseCapabilities(jail, capmask);

  pid_t pid;
  if (!minijail_->RunPipesAndDestroy(
          jail, args, &pid, stdin_fd, stdout_fd, stderr_fd)) {
    LOG(ERROR) << "Unable to spawn " << program.value() << " in a jail.";
    return -1;
  }

  CHECK(process_reaper_.WatchForChild(
      spawn_source,
      pid,
      base::Bind(&ProcessManager::OnProcessExited,
                 weak_factory_.GetWeakPtr(),
                 pid)));

  watched_processes_.emplace(pid, exit_callback);
  return pid;
}

bool ProcessManager::StopProcess(pid_t pid) {
  if (pending_termination_processes_.find(pid) !=
      pending_termination_processes_.end()) {
    LOG(ERROR) << "Process " << pid << " already being stopped.";
    return false;
  }

  if (watched_processes_.find(pid) == watched_processes_.end()) {
    LOG(ERROR) << "Process " << pid << " not being watched";
    return false;
  }
  // Caller not interested in watching this process anymore, since the
  // process termination is initiated by the caller.
  watched_processes_.erase(pid);

  // Attempt to send SIGTERM signal first.
  return TerminateProcess(pid, false);
}

bool ProcessManager::UpdateExitCallback(
    pid_t pid,
    const base::Callback<void(int)>& new_callback) {
  const auto process_entry = watched_processes_.find(pid);
  if (process_entry == watched_processes_.end()) {
    LOG(ERROR) << "Process " << pid << " not being watched";
    return false;
  }

  process_entry->second = new_callback;
  return true;
}

void ProcessManager::OnProcessExited(pid_t pid, const siginfo_t& info) {
  // Invoke the exit callback if the process is being watched.
  auto watched_process = watched_processes_.find(pid);
  if (watched_process != watched_processes_.end()) {
    base::Callback<void(int)> callback = watched_process->second;
    watched_processes_.erase(watched_process);
    callback.Run(info.si_status);
    return;
  }

  // Process terminated by us, cancel timeout handler.
  auto terminated_process = pending_termination_processes_.find(pid);
  if (terminated_process != pending_termination_processes_.end()) {
    terminated_process->second->Cancel();
    pending_termination_processes_.erase(terminated_process);
    return;
  }

  NOTREACHED() << "Unknown process " << pid << " status " << info.si_status;
}

void ProcessManager::ProcessTerminationTimeoutHandler(pid_t pid,
                                                      bool kill_signal) {
  CHECK(pending_termination_processes_.find(pid) !=
        pending_termination_processes_.end());
  pending_termination_processes_.erase(pid);
  // Process still not killed after SIGKILL signal.
  if (kill_signal) {
    LOG(ERROR) << "Timeout waiting for process " << pid << " to be killed.";
    return;
  }

  // Retry using SIGKILL signal.
  TerminateProcess(pid, true);
}

bool ProcessManager::TerminateProcess(pid_t pid, bool kill_signal) {
  int signal = (kill_signal) ? SIGKILL : SIGTERM;
  if (kill(pid, signal) < 0) {
    PLOG(ERROR) << "Failed to send " << signal << " signal to process " << pid;
    return false;
  }
  std::unique_ptr<TerminationTimeoutCallback> termination_callback(
      new TerminationTimeoutCallback(
          base::Bind(&ProcessManager::ProcessTerminationTimeoutHandler,
                     weak_factory_.GetWeakPtr(),
                     pid,
                     kill_signal)));
  dispatcher_->PostDelayedTask(termination_callback->callback(),
                               kTerminationTimeoutSeconds * 1000);
  pending_termination_processes_.emplace(pid, std::move(termination_callback));
  return true;
}

}  // namespace shill
