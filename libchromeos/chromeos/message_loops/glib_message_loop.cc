// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <chromeos/message_loops/glib_message_loop.h>

#include <chromeos/location_logging.h>

using base::Closure;

namespace chromeos {

GlibMessageLoop::GlibMessageLoop() {
  loop_ = g_main_loop_new(g_main_context_default(), FALSE);
}

GlibMessageLoop::~GlibMessageLoop() {
  // Cancel all pending tasks when destroying the message loop.
  for (const auto& task : tasks_) {
    DVLOG_LOC(task.second->location, 1)
        << "Removing task_id " << task.second->task_id
        << " leaked on GlibMessageLoop, scheduled from this location.";
    g_source_remove(task.second->source_id);
  }
  g_main_loop_unref(loop_);
}

MessageLoop::TaskId GlibMessageLoop::PostDelayedTask(
    const tracked_objects::Location& from_here,
    const Closure &task,
    base::TimeDelta delay) {
  MessageLoop::TaskId task_id = ++last_id_;
  if (!task_id)
    task_id = ++last_id_;
  ScheduledTask* scheduled_task = new ScheduledTask{
    this, from_here, task_id, 0, std::move(task)};
  DVLOG_LOC(from_here, 1) << "Scheduling delayed task_id " << task_id
                          << " to run in " << delay << ".";
  scheduled_task->source_id = g_timeout_add_full(
      G_PRIORITY_DEFAULT,
      delay.InMillisecondsRoundedUp(),
      OnRanPostedTask,
      reinterpret_cast<gpointer>(scheduled_task),
      DestroyPostedTask);
  tasks_[task_id] = scheduled_task;
  return task_id;
}

bool GlibMessageLoop::CancelTask(TaskId task_id) {
  if (task_id == kTaskIdNull)
    return false;
  const auto task = tasks_.find(task_id);
  // It is a programmer error to attempt to remove a non-existent source.
  if (task == tasks_.end())
    return false;
  DVLOG_LOC(task->second->location, 1)
      << "Removing task_id " << task_id << " scheduled from this location.";
  guint source_id = task->second->source_id;
  // We remove here the entry from the tasks_ map, the pointer will be deleted
  // by the g_source_remove() call.
  tasks_.erase(task);
  return g_source_remove(source_id);
}

bool GlibMessageLoop::RunOnce(bool may_block) {
  return g_main_context_iteration(nullptr, may_block);
}

void GlibMessageLoop::Run() {
  g_main_loop_run(loop_);
}

void GlibMessageLoop::BreakLoop() {
  g_main_loop_quit(loop_);
}

gboolean GlibMessageLoop::OnRanPostedTask(gpointer user_data) {
  ScheduledTask* scheduled_task = reinterpret_cast<ScheduledTask*>(user_data);
  DVLOG_LOC(scheduled_task->location, 1)
      << "Running task_id " << scheduled_task->task_id
      << " scheduled from this location.";
  // We only need to remove this task_id from the map. DestroyPostedTask will be
  // called with this same |user_data| where we can delete the ScheduledTask.
  scheduled_task->loop->tasks_.erase(scheduled_task->task_id);
  scheduled_task->closure.Run();
  return FALSE;  // Removes the source since a callback can only be called once.
}

void GlibMessageLoop::DestroyPostedTask(gpointer user_data) {
  delete reinterpret_cast<ScheduledTask*>(user_data);
}

}  // namespace chromeos
