// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "camera3_test_thread.h"

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

int Camera3TestThread::PostTaskSync(base::Closure task) {
  auto future = internal::Future<void>::Create(&relay_);
  if (!thread_.task_runner()->PostTask(
          FROM_HERE, base::Bind(&Camera3TestThread::ProcessTaskOnThread,
                                base::Unretained(this), task,
                                internal::GetFutureCallback(future)))) {
    LOG(ERROR) << "Failed to post task";
    return -EIO;
  }
  future->Wait();
  return 0;
}

void Camera3TestThread::ProcessTaskOnThread(const base::Closure& task,
                                            const base::Callback<void()>& cb) {
  task.Run();
  cb.Run();
}

}  // namespace camera3_test
