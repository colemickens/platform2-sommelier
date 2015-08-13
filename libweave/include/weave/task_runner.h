// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBWEAVE_INCLUDE_WEAVE_TASK_RUNNER_H_
#define LIBWEAVE_INCLUDE_WEAVE_TASK_RUNNER_H_

#include <base/message_loop/message_loop.h>
#include <chromeos/message_loops/message_loop.h>

namespace weave {

using TaskRunner = chromeos::MessageLoop;

}  // namespace weave

#endif  // LIBWEAVE_INCLUDE_WEAVE_TASK_RUNNER_H_
