// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CROS_DISKS_GLIB_PROCESS_H_
#define CROS_DISKS_GLIB_PROCESS_H_

#include <glib.h>
#include <sys/types.h>

#include <base/callback.h>

#include "cros-disks/process.h"

namespace cros_disks {

// An implementation of Process that uses Glib to execute a process.
class GlibProcess : public Process {
 public:
  // Callback upon the completion or termination of a process.
  typedef base::Callback<void(GlibProcess* process)> Callback;

  GlibProcess();
  virtual ~GlibProcess();

  // Implements Process::Start() to start a process without waiting
  // for it to terminate. Return true on success.
  virtual bool Start();

  pid_t pid() const { return pid_; }

  int status() const { return status_; }

  void set_callback(const Callback& callback) { callback_ = callback; }

 private:
  // Called by OnChildWatchNotify() to set the process termination status
  // and perform proper cleanup.
  void OnTerminated(int status);

  // Called by g_child_watch_add() upon process termination.
  static void OnChildWatchNotify(GPid pid, gint status, gpointer data);

  // Process ID (default to 0 when the process has not started).
  pid_t pid_;

  // Termination status of the process (default to 0 if the process has not
  // started). See wait(2) for details.
  int status_;

  // Glib specific event source ID returned by g_child_watch_add().
  guint child_watch_id_;

  // Callback to invoke when the process terminates.
  Callback callback_;

  DISALLOW_COPY_AND_ASSIGN(GlibProcess);
};

}  // namespace cros_disks

#endif  // CROS_DISKS_GLIB_PROCESS_H_
