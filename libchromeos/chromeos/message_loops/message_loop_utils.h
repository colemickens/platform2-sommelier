// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBCHROMEOS_CHROMEOS_MESSAGE_LOOPS_MESSAGE_LOOP_UTILS_H_
#define LIBCHROMEOS_CHROMEOS_MESSAGE_LOOPS_MESSAGE_LOOP_UTILS_H_

#include <base/callback.h>
#include <base/time/time.h>

#include <chromeos/chromeos_export.h>
#include <chromeos/message_loops/message_loop.h>

namespace chromeos {

// Run the MessageLoop until the condition passed in |terminate| returns true
// or the timeout expires.
CHROMEOS_EXPORT void MessageLoopRunUntil(
    MessageLoop* loop,
    base::TimeDelta timeout,
    base::Callback<bool()> terminate);

// Run the MessageLoop |loop| for up to |iterations| times without blocking.
// Return the number of tasks run.
CHROMEOS_EXPORT int MessageLoopRunMaxIterations(MessageLoop* loop,
                                                int iterations);

}  // namespace chromeos

#endif  // LIBCHROMEOS_CHROMEOS_MESSAGE_LOOPS_MESSAGE_LOOP_UTILS_H_
