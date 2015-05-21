// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GERM_TEST_UTIL_H_
#define GERM_TEST_UTIL_H_

#include <signal.h>

#include <string>

#include <base/message_loop/message_loop.h>
#include <gmock/gmock.h>

#include "germ/proto_bindings/soma_sandbox_spec.pb.h"

namespace germ {

// Sends SIGTERM to all processes in the current process's process group if this
// object is alive for |seconds|.
class ScopedAlarm {
 public:
  explicit ScopedAlarm(unsigned int seconds);
  ~ScopedAlarm();

 private:
  struct sigaction oldact_;
};

// Matcher for SandboxSpecs. Only checks whether names match.
MATCHER_P(EqualsSpec, expected, "") {
  return expected.name() == arg.name();
}

soma::SandboxSpec MakeSpecForTest(const std::string& name);

ACTION_P(PostTask, closure) {
  base::MessageLoop::current()->task_runner()->PostTask(FROM_HERE, closure);
}

}  // namespace germ

#endif  // GERM_TEST_UTIL_H_
