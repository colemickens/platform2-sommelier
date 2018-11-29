// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HERMES_EXECUTOR_H_
#define HERMES_EXECUTOR_H_

#include <base/message_loop/message_loop.h>
#include <google-lpa/lpa/util/executor.h>

namespace hermes {

// Class to allow an arbitrary std::function<void()> to be executed on the
// thread of the provided MessageLoop.
class Executor : public lpa::util::Executor {
 public:
  explicit Executor(base::MessageLoop* message_loop);
  void Execute(std::function<void()> f) override;

 private:
  base::MessageLoop* message_loop_;
};

}  // namespace hermes

#endif  // HERMES_EXECUTOR_H_
