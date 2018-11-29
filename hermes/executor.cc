// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "hermes/executor.h"

#include <utility>

#include <base/bind.h>
#include <base/logging.h>
#include <base/message_loop/message_loop.h>

namespace hermes {

Executor::Executor(base::MessageLoop* message_loop)
  : message_loop_(message_loop) {
  CHECK(message_loop_);
}

void Executor::Execute(std::function<void()> f) {
  // TaskRunner::PostTask takes a base::Closure, not a std::function. Convert
  // the captureless lambda into a base::BindState for use as a base::Closure.
  base::Closure task = base::Bind([](std::function<void()> f){ f(); },
                                  std::move(f));
  message_loop_->task_runner()->PostTask(FROM_HERE, std::move(task));
}

}  // namespace hermes
