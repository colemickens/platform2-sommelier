// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBCHROMEOS_CHROMEOS_MESSAGE_LOOPS_GLIB_MESSAGE_LOOP_H_
#define LIBCHROMEOS_CHROMEOS_MESSAGE_LOOPS_GLIB_MESSAGE_LOOP_H_

#include <map>
#include <memory>

#include <base/location.h>
#include <base/time/time.h>
#include <glib.h>

#include <chromeos/chromeos_export.h>
#include <chromeos/message_loops/message_loop.h>

namespace chromeos {

class CHROMEOS_EXPORT GlibMessageLoop : public MessageLoop {
 public:
  GlibMessageLoop();
  ~GlibMessageLoop() override;

  // MessageLoop overrides.
  MessageLoop::TaskId PostDelayedTask(
      const tracked_objects::Location& from_here,
      const base::Closure &task,
      base::TimeDelta delay) override;
  using MessageLoop::PostDelayedTask;
  bool CancelTask(TaskId task_id) override;
  bool RunOnce(bool may_block) override;
  void Run() override;
  void BreakLoop() override;

 private:
  // Called by the GLib's main loop when is time to call the callback scheduled
  // with Post*Task(). The pointer to the callback passed when scheduling it is
  // passed to this function as a gpointer on |user_data|.
  static gboolean OnRanPostedTask(gpointer user_data);

  // Called by the GLib's main loop when the scheduled callback is removed due
  // to it being executed or canceled.
  static void DestroyPostedTask(gpointer user_data);

  GMainLoop* loop_;

  struct ScheduledTask {
    // A pointer to this GlibMessageLoop so we can remove the Task from the
    // glib callback.
    GlibMessageLoop* loop;
    tracked_objects::Location location;

    MessageLoop::TaskId task_id;
    guint source_id;
    base::Closure closure;
  };

  std::map<MessageLoop::TaskId, ScheduledTask*> tasks_;

  MessageLoop::TaskId last_id_ = kTaskIdNull;

  DISALLOW_COPY_AND_ASSIGN(GlibMessageLoop);
};

}  // namespace chromeos

#endif  // LIBCHROMEOS_CHROMEOS_MESSAGE_LOOPS_GLIB_MESSAGE_LOOP_H_
