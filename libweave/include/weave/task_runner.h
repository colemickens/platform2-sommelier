// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBWEAVE_INCLUDE_WEAVE_TASK_RUNNER_H_
#define LIBWEAVE_INCLUDE_WEAVE_TASK_RUNNER_H_

#include <string>
#include <utility>
#include <vector>

#include <base/callback.h>
#include <base/location.h>
#include <base/time/time.h>

namespace weave {

class TaskRunner {
 public:
  virtual void PostDelayedTask(const tracked_objects::Location& from_here,
                               const base::Closure& task,
                               base::TimeDelta delay) = 0;

 protected:
  virtual ~TaskRunner() = default;
};

}  // namespace weave

#endif  // LIBWEAVE_INCLUDE_WEAVE_TASK_RUNNER_H_
