// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/process_killer.h"

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
ProcessKiller *ProcessKiller::GetInstance() {
  return g_process_killer.Pointer();
}

void ProcessKiller::Kill(int pid, const Closure &callback) {
  LOG(INFO) << "Killing pid " << pid;
  if (!callback.is_null()) {
    callbacks_[pid] = callback;
  }
  g_child_watch_add(pid, OnProcessDied, this);
  // TODO(petkov): Consider sending subsequent periodic signals and raising the
  // signal to SIGKILL if the process keeps running.
  kill(pid, SIGTERM);
}

// static
void ProcessKiller::OnProcessDied(GPid pid, gint status, gpointer data) {
  LOG(INFO) << "pid " << pid << " died, status " << status;
  ProcessKiller *me = reinterpret_cast<ProcessKiller *>(data);
  map<int, Closure>::iterator callback_it = me->callbacks_.find(pid);
  if (callback_it == me->callbacks_.end()) {
    return;
  }
  const Closure &callback = callback_it->second;
  if (!callback.is_null()) {
    LOG(INFO) << "Running callback for dead pid " << pid;
    callback.Run();
  }
  me->callbacks_.erase(callback_it);
}

}  // namespace shill
