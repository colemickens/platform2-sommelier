// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mist/event_dispatcher.h"

#include <utility>

#include <base/location.h>
#include <base/run_loop.h>
#include <base/strings/stringprintf.h>
#include <base/threading/thread_task_runner_handle.h>

namespace mist {

EventDispatcher::EventDispatcher()
    : task_runner_(base::ThreadTaskRunnerHandle::Get()) {}

EventDispatcher::~EventDispatcher() = default;

void EventDispatcher::DispatchForever() {
  base::RunLoop run_loop;
  quit_closure_ = run_loop.QuitWhenIdleClosure();
  run_loop.Run();
}

void EventDispatcher::Stop() {
  task_runner_->PostTask(FROM_HERE, std::move(quit_closure_));
}

bool EventDispatcher::PostTask(const base::Closure& task) {
  return task_runner_->PostTask(FROM_HERE, task);
}

bool EventDispatcher::PostDelayedTask(const base::Closure& task,
                                      const base::TimeDelta& delay) {
  return task_runner_->PostDelayedTask(FROM_HERE, task, delay);
}

bool EventDispatcher::StartWatchingFileDescriptor(
    int file_descriptor, Mode mode, const base::RepeatingClosure& callback) {
  CHECK_GE(file_descriptor, 0);

  Watcher& watcher = file_descriptor_watchers_[file_descriptor];
  // Reset once if it is already being watched.
  watcher.read_watcher = nullptr;
  watcher.write_watcher = nullptr;

  bool success = true;
  if (mode == Mode::READ || mode == Mode::READ_WRITE) {
    watcher.read_watcher =
        base::FileDescriptorWatcher::WatchReadable(file_descriptor, callback);
    success = success && watcher.read_watcher.get();
  }
  if (mode == Mode::WRITE || mode == Mode::READ_WRITE) {
    watcher.write_watcher =
        base::FileDescriptorWatcher::WatchWritable(file_descriptor, callback);
    success = success && watcher.write_watcher.get();
  }

  if (!success) {
    LOG(ERROR) << "Could not watch file descriptor: " << file_descriptor;
    file_descriptor_watchers_.erase(file_descriptor);
    return false;
  }

  VLOG(2) << "Started watching file descriptor: " << file_descriptor;
  return true;
}

bool EventDispatcher::StopWatchingFileDescriptor(int file_descriptor) {
  CHECK_GE(file_descriptor, 0);

  auto it = file_descriptor_watchers_.find(file_descriptor);
  if (it == file_descriptor_watchers_.end()) {
    LOG(ERROR) << "File descriptor " << file_descriptor
               << " is not being watched.";
    return false;
  }

  file_descriptor_watchers_.erase(it);
  VLOG(2) << "Stopped watching file descriptor: " << file_descriptor;
  return true;
}

void EventDispatcher::StopWatchingAllFileDescriptors() {
  file_descriptor_watchers_.clear();
}

}  // namespace mist
