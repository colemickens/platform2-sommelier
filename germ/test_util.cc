// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "germ/test_util.h"

#include <signal.h>
#include <sys/types.h>
#include <unistd.h>

#include <string>

#include <base/logging.h>

#include "germ/proto_bindings/soma_sandbox_spec.pb.h"

namespace germ {

namespace {

void SIGALRMHandler(int sig) {
  LOG(ERROR) << "ScopedAlarm timed out!";
  PCHECK(kill(-getpgrp(), SIGTERM) == 0);
  NOTREACHED();
}

}  // namespace

ScopedAlarm::ScopedAlarm(unsigned int seconds) {
  struct sigaction act = {};
  act.sa_handler = &SIGALRMHandler;
  PCHECK(sigaction(SIGALRM, &act, &oldact_) == 0);
  alarm(seconds);
}

ScopedAlarm::~ScopedAlarm() {
  alarm(0);
  PCHECK(sigaction(SIGALRM, &oldact_, nullptr) == 0);
}

soma::SandboxSpec MakeSpecForTest(const std::string& name) {
  soma::SandboxSpec spec;
  spec.set_name(name);
  return spec;
}

}  // namespace germ
