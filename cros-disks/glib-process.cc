// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cros-disks/glib-process.h"

#include <base/logging.h>

namespace cros_disks {

GlibProcess::GlibProcess() : status_(0), child_watch_id_(0) {
}

GlibProcess::~GlibProcess() {
  // If the process has not yet terminated or |callback_| has not been called,
  // we need to remove the event source returned by g_child_watch_add() to
  // prevent |callback_| from being triggered after this object is destructed.
  if (child_watch_id_ != 0) {
    g_source_remove(child_watch_id_);
    child_watch_id_ = 0;

    g_spawn_close_pid(pid());
    set_pid(kInvalidProcessId);
  }
}

bool GlibProcess::Start() {
  CHECK_EQ(kInvalidProcessId, pid()) << "Process has already started.";

  char** arguments = GetArguments();
  CHECK(arguments) << "No argument is provided.";

  GSpawnFlags flags = static_cast<GSpawnFlags>(
      G_SPAWN_DO_NOT_REAP_CHILD |  // Required for g_child_watch_add to work
      G_SPAWN_SEARCH_PATH |
      G_SPAWN_STDOUT_TO_DEV_NULL |
      G_SPAWN_STDERR_TO_DEV_NULL);
  pid_t child_pid = kInvalidProcessId;
  GError* error = NULL;
  gboolean success = g_spawn_async(NULL,  // Inherit parent's working directory
                                   arguments,
                                   NULL,  // Inherit parent's environment
                                   flags,
                                   NULL,  // No setup function
                                   NULL,  // No user data
                                   &child_pid,
                                   &error);

  if (success) {
    set_pid(child_pid);
    child_watch_id_ =
        g_child_watch_add(child_pid, &GlibProcess::OnChildWatchNotify, this);
  }
  if (error) {
    LOG(ERROR) << "Failed to spawn a process: " << error->message;
    g_error_free(error);
  }
  return success;
}

void GlibProcess::OnTerminated(int status) {
  status_ = status;
  child_watch_id_ = 0;

  char** arguments = GetArguments();
  CHECK(arguments);

  if (WIFEXITED(status)) {
    LOG(INFO) << "Process '" << arguments[0] << "' (pid " << pid()
              << ") terminated normally with an exit status "
              << WEXITSTATUS(status) << ".";
  } else if (WIFSIGNALED(status)) {
    LOG(INFO) << "Process '" << arguments[0] << "' (pid " << pid()
              << ") terminated by a signal " << WTERMSIG(status) << ".";
  }

  if (!callback_.is_null())
    callback_.Run(this);
}

// static
void GlibProcess::OnChildWatchNotify(GPid pid, gint status, gpointer data) {
  cros_disks::GlibProcess* process =
      static_cast<cros_disks::GlibProcess*>(data);
  CHECK(process);
  CHECK_EQ(process->pid(), pid);
  process->OnTerminated(status);
  g_spawn_close_pid(pid);
}

}  // namespace cros_disks
