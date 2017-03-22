// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAMERA3_TEST_CAMERA3_TEST_THREAD_H
#define CAMERA3_TEST_CAMERA3_TEST_THREAD_H

#include <arc/future.h>
#include <base/threading/thread.h>

namespace camera3_test {

class Camera3TestThread final {
 public:
  explicit Camera3TestThread(const char* name) : thread_(name) {}

  // Starts the thread. Returns true if the thread was successfully started.
  bool Start();

  // Stop the thread. This function is expected to be called explicitly. A fatal
  // error would have occured in the AtExitManager if it were called in
  // the destructor.
  void Stop();

  // Posts the given task to be run and wait till it is finished
  int PostTaskSync(base::Closure task);

 private:
  void ProcessTaskOnThread(const base::Closure& task,
                           const base::Callback<void()>& cb);

  base::Thread thread_;

  internal::CancellationRelay relay_;

  DISALLOW_COPY_AND_ASSIGN(Camera3TestThread);
};

}  // namespace camera3_test

#endif  // CAMERA3_TEST_CAMERA3_TEST_THREAD_H
