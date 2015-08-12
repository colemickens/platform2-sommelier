// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <chromeos/message_loops/base_message_loop.h>

#include <fcntl.h>
#include <unistd.h>

#include <base/bind.h>
#include <base/run_loop.h>

#include <chromeos/location_logging.h>

using base::Closure;

namespace chromeos {

BaseMessageLoop::BaseMessageLoop(base::MessageLoopForIO* base_loop)
    : base_loop_(base_loop),
      weak_ptr_factory_(this) {}

BaseMessageLoop::~BaseMessageLoop() {
  for (auto& io_task : io_tasks_) {
    DVLOG_LOC(io_task.second.location(), 1)
        << "Removing file descriptor watcher task_id " << io_task.first
        << " leaked on BaseMessageLoop, scheduled from this location.";
    io_task.second.StopWatching();
  }

  // Note all pending canceled delayed tasks when destroying the message loop.
  size_t lazily_deleted_tasks = 0;
  for (const auto& delayed_task : delayed_tasks_) {
    if (delayed_task.second.closure.is_null()) {
      lazily_deleted_tasks++;
    } else {
      DVLOG_LOC(delayed_task.second.location, 1)
          << "Removing delayed task_id " << delayed_task.first
          << " leaked on BaseMessageLoop, scheduled from this location.";
    }
  }
  if (lazily_deleted_tasks) {
    LOG(INFO) << "Leaking " << lazily_deleted_tasks << " canceled tasks.";
  }
}

MessageLoop::TaskId BaseMessageLoop::PostDelayedTask(
    const tracked_objects::Location& from_here,
    const Closure &task,
    base::TimeDelta delay) {
  TaskId task_id =  NextTaskId();
  bool base_scheduled = base_loop_->task_runner()->PostDelayedTask(
      from_here,
      base::Bind(&BaseMessageLoop::OnRanPostedTask,
                 weak_ptr_factory_.GetWeakPtr(),
                 task_id),
      delay);
  DVLOG_LOC(from_here, 1) << "Scheduling delayed task_id " << task_id
                          << " to run in " << delay << ".";
  if (!base_scheduled)
    return MessageLoop::kTaskIdNull;

  delayed_tasks_.emplace(task_id,
                         DelayedTask{from_here, task_id, std::move(task)});
  return task_id;
}

MessageLoop::TaskId BaseMessageLoop::WatchFileDescriptor(
    const tracked_objects::Location& from_here,
    int fd,
    WatchMode mode,
    bool persistent,
    const Closure &task) {
  // base::MessageLoopForIO CHECKS that "fd >= 0", so we handle that case here.
  if (fd < 0)
    return MessageLoop::kTaskIdNull;

  base::MessageLoopForIO::Mode base_mode = base::MessageLoopForIO::WATCH_READ;
  switch (mode) {
    case MessageLoop::kWatchRead:
      base_mode = base::MessageLoopForIO::WATCH_READ;
      break;
    case MessageLoop::kWatchWrite:
      base_mode = base::MessageLoopForIO::WATCH_WRITE;
      break;
    default:
      return MessageLoop::kTaskIdNull;
  }

  TaskId task_id =  NextTaskId();
  auto it_bool = io_tasks_.emplace(
      std::piecewise_construct,
      std::forward_as_tuple(task_id),
      std::forward_as_tuple(
          from_here, this, task_id, fd, base_mode, persistent, task));
  // This should always insert a new element.
  DCHECK(it_bool.second);
  bool scheduled = it_bool.first->second.StartWatching();
  DVLOG_LOC(from_here, 1)
      << "Watching fd " << fd << " for "
      << (mode == MessageLoop::kWatchRead ? "reading" : "writing")
      << (persistent ? " persistently" : " just once")
      << " as task_id " << task_id
      << (scheduled ? " successfully" : " failed.");

  if (!scheduled) {
    io_tasks_.erase(task_id);
    return MessageLoop::kTaskIdNull;
  }
  return task_id;
}

bool BaseMessageLoop::CancelTask(TaskId task_id) {
  if (task_id == kTaskIdNull)
    return false;
  auto delayed_task_it = delayed_tasks_.find(task_id);
  if (delayed_task_it == delayed_tasks_.end()) {
    // This might be an IOTask then.
    auto io_task_it = io_tasks_.find(task_id);
    if (io_task_it == io_tasks_.end())
      return false;
    return io_task_it->second.CancelTask();
  }
  // A DelayedTask was found for this task_id at this point.

  // Check if the callback was already canceled but we have the entry in
  // delayed_tasks_ since it didn't fire yet in the message loop.
  if (delayed_task_it->second.closure.is_null())
    return false;

  DVLOG_LOC(delayed_task_it->second.location, 1)
      << "Removing task_id " << task_id << " scheduled from this location.";
  // We reset to closure to a null Closure to release all the resources
  // used by this closure at this point, but we don't remove the task_id from
  // delayed_tasks_ since we can't tell base::MessageLoopForIO to not run it.
  delayed_task_it->second.closure = Closure();

  return true;
}

bool BaseMessageLoop::RunOnce(bool may_block) {
  run_once_ = true;
  base::RunLoop run_loop;  // Uses the base::MessageLoopForIO implicitly.
  base_run_loop_ = &run_loop;
  if (!may_block)
    run_loop.RunUntilIdle();
  else
    run_loop.Run();
  base_run_loop_ = nullptr;
  // If the flag was reset to false, it means a closure was run.
  if (!run_once_)
    return true;

  run_once_ = false;
  return false;
}

void BaseMessageLoop::Run() {
  base::RunLoop run_loop;  // Uses the base::MessageLoopForIO implicitly.
  base_run_loop_ = &run_loop;
  run_loop.Run();
  base_run_loop_ = nullptr;
}

void BaseMessageLoop::BreakLoop() {
  if (base_run_loop_ == nullptr) {
    DVLOG(1) << "Message loop not running, ignoring BreakLoop().";
    return;  // Message loop not running, nothing to do.
  }
  base_run_loop_->Quit();
}

Closure BaseMessageLoop::QuitClosure() const {
  if (base_run_loop_ == nullptr)
    return base::Bind(&base::DoNothing);
  return base_run_loop_->QuitClosure();
}

MessageLoop::TaskId BaseMessageLoop::NextTaskId() {
  TaskId res;
  do {
    res = ++last_id_;
    // We would run out of memory before we run out of task ids.
  } while (!res ||
           delayed_tasks_.find(res) != delayed_tasks_.end() ||
           io_tasks_.find(res) != io_tasks_.end());
  return res;
}

void BaseMessageLoop::OnRanPostedTask(MessageLoop::TaskId task_id) {
  auto task_it = delayed_tasks_.find(task_id);
  DCHECK(task_it != delayed_tasks_.end());
  if (!task_it->second.closure.is_null()) {
    DVLOG_LOC(task_it->second.location, 1)
        << "Running delayed task_id " << task_id
        << " scheduled from this location.";
    // Mark the task as canceled while we are running it so CancelTask returns
    // false.
    Closure closure = std::move(task_it->second.closure);
    task_it->second.closure = Closure();
    closure.Run();

    // If the |run_once_| flag is set, it is because we are instructed to run
    // only once callback.
    if (run_once_) {
      run_once_ = false;
      BreakLoop();
    }
  }
  delayed_tasks_.erase(task_it);
}

void BaseMessageLoop::OnFileReadyPostedTask(MessageLoop::TaskId task_id) {
  auto task_it = io_tasks_.find(task_id);
  // Even if this task was canceled while we were waiting in the message loop
  // for this method to run, the entry in io_tasks_ should still be present, but
  // won't do anything.
  DCHECK(task_it != io_tasks_.end());
  task_it->second.OnFileReadyPostedTask();
}

BaseMessageLoop::IOTask::IOTask(const tracked_objects::Location& location,
                                BaseMessageLoop* loop,
                                MessageLoop::TaskId task_id,
                                int fd,
                                base::MessageLoopForIO::Mode base_mode,
                                bool persistent,
                                const Closure& task)
    : location_(location), loop_(loop), task_id_(task_id),
      fd_(fd), base_mode_(base_mode), persistent_(persistent), closure_(task) {}

bool BaseMessageLoop::IOTask::StartWatching() {
  return loop_->base_loop_->WatchFileDescriptor(
      fd_, persistent_, base_mode_, &fd_watcher_, this);
}

void BaseMessageLoop::IOTask::StopWatching() {
  // This is safe to call even if we are not watching for it.
  fd_watcher_.StopWatchingFileDescriptor();
}

void BaseMessageLoop::IOTask::OnFileCanReadWithoutBlocking(int /* fd */) {
  OnFileReady();
}

void BaseMessageLoop::IOTask::OnFileCanWriteWithoutBlocking(int /* fd */) {
  OnFileReady();
}

void BaseMessageLoop::IOTask::OnFileReady() {
  // When the file descriptor becomes available we stop watching for it and
  // schedule a task to run the callback from the main loop. The callback will
  // run using the same scheduler use to run other delayed tasks, avoiding
  // starvation of the available posted tasks if there are file descriptors
  // always available. The new posted task will use the same TaskId as the
  // current file descriptor watching task an could be canceled in either state,
  // when waiting for the file descriptor or waiting in the main loop.
  StopWatching();
  bool base_scheduled = loop_->base_loop_->task_runner()->PostTask(
      location_,
      base::Bind(&BaseMessageLoop::OnFileReadyPostedTask,
                 loop_->weak_ptr_factory_.GetWeakPtr(),
                 task_id_));
  posted_task_pending_ = true;
  if (base_scheduled) {
    DVLOG_LOC(location_, 1)
        << "Dispatching task_id " << task_id_ << " for "
        << (base_mode_ == base::MessageLoopForIO::WATCH_READ ?
            "reading" : "writing")
        << " file descriptor " << fd_ << ", scheduled from this location.";
  } else {
    // In the rare case that PostTask() fails, we fall back to run it directly.
    // This would indicate a bigger problem with the message loop setup.
    LOG(ERROR) << "Error on base::MessageLoopForIO::PostTask().";
    OnFileReadyPostedTask();
  }
}

void BaseMessageLoop::IOTask::OnFileReadyPostedTask() {
  // We can't access |this| after running the |closure_| since it could call
  // CancelTask on its own task_id, so we copy the members we need now.
  BaseMessageLoop* loop_ptr = loop_;
  DCHECK(posted_task_pending_ = true);
  posted_task_pending_ = false;

  // If this task was already canceled, the closure will be null and there is
  // nothing else to do here. This execution doesn't count a step for RunOnce()
  // unless we have a callback to run.
  if (closure_.is_null()) {
    loop_->io_tasks_.erase(task_id_);
    return;
  }

  DVLOG_LOC(location_, 1)
      << "Running task_id " << task_id_ << " for "
      << (base_mode_ == base::MessageLoopForIO::WATCH_READ ?
          "reading" : "writing")
      << " file descriptor " << fd_ << ", scheduled from this location.";

  if (persistent_) {
    // In the persistent case we just run the callback. If this callback cancels
    // the task id, we can't access |this| anymore, so we re-start watching the
    // file descriptor before running the callback.
    StartWatching();
    closure_.Run();
  } else {
    // This will destroy |this|, the fd_watcher and therefore stop watching this
    // file descriptor.
    Closure closure_copy = std::move(closure_);
    loop_->io_tasks_.erase(task_id_);
    // Run the closure from the local copy we just made.
    closure_copy.Run();
  }

  if (loop_ptr->run_once_) {
    loop_ptr->run_once_ = false;
    loop_ptr->BreakLoop();
  }
}

bool BaseMessageLoop::IOTask::CancelTask() {
  if (closure_.is_null())
    return false;

  DVLOG_LOC(location_, 1)
      << "Removing task_id " << task_id_ << " scheduled from this location.";

  if (!posted_task_pending_) {
    // Destroying the FileDescriptorWatcher implicitly stops watching the file
    // descriptor. This will delete our instance.
    loop_->io_tasks_.erase(task_id_);
    return true;
  }
  // The IOTask is waiting for the message loop to run its delayed task, so
  // it is not watching for the file descriptor. We release the closure
  // resources now but keep the IOTask instance alive while we wait for the
  // callback to run and delete the IOTask.
  closure_ = Closure();
  return true;
}

}  // namespace chromeos
