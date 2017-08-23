// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "camera3_test/camera3_test_thread.h"

#include <base/logging.h>

namespace camera3_test {

bool Camera3TestThread::Start() {
  if (!thread_.Start()) {
    LOG(ERROR) << "Failed to start thread";
    return false;
  }
  thread_.WaitUntilThreadStarted();
  return true;
}

void Camera3TestThread::Stop() {
  thread_.Stop();
}

int Camera3TestThread::PostTaskSync(const tracked_objects::Location& from_here,
                                    base::Closure task) {
  VLOGF_ENTER();
  auto future = internal::Future<void>::Create(&relay_);
  if (!thread_.task_runner() ||
      !thread_.task_runner()->PostTask(
          from_here, base::Bind(&Camera3TestThread::ProcessTaskOnThread,
                                base::Unretained(this), task,
                                internal::GetFutureCallback(future)))) {
    LOG(ERROR) << "Failed to post task";
    return -EIO;
  }
  future->Wait();
  return 0;
}

int Camera3TestThread::PostTaskAsync(const tracked_objects::Location& from_here,
                                     base::Closure task) {
  VLOGF_ENTER();
  if (!thread_.task_runner() ||
      !thread_.task_runner()->PostTask(from_here, task)) {
    LOG(ERROR) << "Failed to post task";
    return -EIO;
  }
  return 0;
}

bool Camera3TestThread::IsCurrentThread() const {
  return thread_.GetThreadId() == base::PlatformThread::CurrentId();
}

void Camera3TestThread::ProcessTaskOnThread(const base::Closure& task,
                                            const base::Callback<void()>& cb) {
  VLOGF_ENTER();
  task.Run();
  cb.Run();
}

}  // namespace camera3_test
