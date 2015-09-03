//
// Copyright (C) 2012 The Android Open Source Project
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

#include "shill/process_killer.h"

#include <errno.h>
#include <sys/wait.h>

using base::Closure;
using std::map;

namespace shill {

namespace {

base::LazyInstance<ProcessKiller> g_process_killer = LAZY_INSTANCE_INITIALIZER;

}  // namespace

ProcessKiller::ProcessKiller() {}

ProcessKiller::~ProcessKiller() {
  // There's no need to release any GLib child watch sources because this class
  // is a singleton and will be destroyed when this process exits, i.e., when
  // GLib is shut down.
}

// static
ProcessKiller* ProcessKiller::GetInstance() {
  return g_process_killer.Pointer();
}

bool ProcessKiller::Wait(int pid, const Closure& callback) {
  LOG(INFO) << "Waiting for pid " << pid;
  if (!callback.is_null()) {
    callbacks_[pid] = callback;
  }
  // Check if the child process is dead already. This guards against the case
  // when the caller had registered a child watch on that process but the
  // process exited before the caller removed the watch and invoked this.
  int status = 0;
  pid_t rpid = waitpid(pid, &status, WNOHANG);
  if (rpid == -1) {
    DCHECK_EQ(ECHILD, errno);
    LOG(INFO) << "No such child -- assuming process has already exited.";
    OnProcessDied(pid, 0, this);
    return false;
  }
  if (rpid == pid) {
    LOG(INFO) << "Process has already exited.";
    OnProcessDied(pid, status, this);
    return false;
  }
  g_child_watch_add(pid, OnProcessDied, this);
  return true;
}

void ProcessKiller::Kill(int pid, const Closure& callback) {
  if (!Wait(pid, callback)) {
    LOG(INFO) << "Process already dead, no need to kill.";
    return;
  }
  LOG(INFO) << "Killing pid " << pid;
  // TODO(petkov): Consider sending subsequent periodic signals and raising the
  // signal to SIGKILL if the process keeps running.
  if (kill(pid, SIGTERM) == -1) {
    PLOG(ERROR) << "SIGTERM failed";
  }
}

// static
void ProcessKiller::OnProcessDied(GPid pid, gint status, gpointer data) {
  LOG(INFO) << "pid " << pid << " died, status " << status;
  ProcessKiller* me = reinterpret_cast<ProcessKiller*>(data);
  map<int, Closure>::iterator callback_it = me->callbacks_.find(pid);
  if (callback_it == me->callbacks_.end()) {
    return;
  }
  const Closure& callback = callback_it->second;
  if (!callback.is_null()) {
    LOG(INFO) << "Running callback for dead pid " << pid;
    callback.Run();
  }
  me->callbacks_.erase(callback_it);
}

}  // namespace shill
