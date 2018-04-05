/*
 * Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef INCLUDE_CROS_CAMERA_CAMERA_THREAD_H_
#define INCLUDE_CROS_CAMERA_CAMERA_THREAD_H_

#include <string>
#include <unordered_map>

#include <base/threading/thread.h>

#include "cros-camera/common.h"
#include "cros-camera/export.h"
#include "cros-camera/future.h"

namespace cros {

class CROS_CAMERA_EXPORT CameraThread {
 public:
  explicit CameraThread(std::string name) : thread_(name) {}

  // Starts the thread. Returns true if the thread was successfully started.
  bool Start() {
    if (!thread_.Start()) {
      LOG(ERROR) << "Failed to start thread";
      return false;
    }
    thread_.WaitUntilThreadStarted();
    return true;
  }

  // Stop the thread. This function is expected to be called explicitly. A fatal
  // error would have occured in the AtExitManager if it were called in
  // the destructor.
  void Stop() { thread_.Stop(); }

  // Returns true if this is the current thread that is running.
  bool IsCurrentThread() const {
    return thread_.GetThreadId() == base::PlatformThread::CurrentId();
  }

  // Posts the given task to be run and wait till it is finished. The reply
  // from callee will be stored in |result|. Return 0 if succeed. Otherwise
  // return -EIO.
  template <typename T>
  int PostTaskSync(const tracked_objects::Location& from_here,
                   base::Callback<T()> task,
                   T* result) {
    VLOGF_ENTER();
    if (!thread_.task_runner()) {
      LOG(ERROR) << "Thread is not started";
      return -EIO;
    }

    auto future = cros::Future<T>::Create(nullptr);
    base::Closure closure = base::Bind(
        &CameraThread::ProcessSyncTaskOnThread<T>,
        base::Unretained(this), task, future);
    if (!thread_.task_runner()->PostTask(from_here, closure)) {
      LOG(ERROR) << "Failed to post task";
      return -EIO;
    }

    *result = future->Get();
    return 0;
  }

  // Posts the given task to be run asynchronously. Return 0 if succeed.
  // Otherwise return -EIO.
  template <typename T>
  int PostTaskAsync(const tracked_objects::Location& from_here,
                    base::Callback<T()> task) {
    VLOGF_ENTER();
    if (!thread_.task_runner()) {
      LOG(ERROR) << "Thread is not started";
      return -EIO;
    }
    base::Closure closure = base::Bind(
        &CameraThread::ProcessASyncTaskOnThread<T>,
        base::Unretained(this), task);

    if (!thread_.task_runner()->PostTask(from_here, closure)) {
      LOG(ERROR) << "Failed to post task";
      return -EIO;
    }
    return 0;
  }

  // Posts the given task to be run and wait till it is finished.
  // Return 0 if succeed. Otherwise return -EIO.
  int PostTaskSync(const tracked_objects::Location& from_here,
                   base::Closure task) {
    VLOGF_ENTER();
    if (!thread_.task_runner()) {
      LOG(ERROR) << "Thread is not started";
      return -EIO;
    }

    auto future = cros::Future<void>::Create(nullptr);
    base::Closure closure = base::Bind(
        &CameraThread::ProcessClosureSyncTaskOnThread,
        base::Unretained(this), task, future);
    if (!thread_.task_runner()->PostTask(from_here, closure)) {
      LOG(ERROR) << "Failed to post task";
      return -EIO;
    }

    future->Wait();
    return 0;
  }

 private:
  template <typename T>
  void ProcessSyncTaskOnThread(const base::Callback<T()>& task,
                               scoped_refptr<cros::Future<T>> future) {
    VLOGF_ENTER();
    auto result = task.Run();
    future->Set(result);
  }

  template <typename T>
  void ProcessASyncTaskOnThread(const base::Callback<T()>& task) {
    VLOGF_ENTER();
    task.Run();
  }

  void ProcessClosureSyncTaskOnThread(
      const base::Closure& task, scoped_refptr<cros::Future<void>> future) {
    VLOGF_ENTER();
    task.Run();
    future->Set();
  }

  base::Thread thread_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(CameraThread);
};

}  // namespace cros

#endif  // INCLUDE_CROS_CAMERA_CAMERA_THREAD_H_
