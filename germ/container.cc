// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "germ/container.h"

#include <sys/types.h>

#include <base/bind.h>
#include <base/location.h>
#include <base/logging.h>
#include <base/time/time.h>

namespace germ {

const pid_t kInvalidPid = -1;

Container::Container(const soma::SandboxSpec& spec)
    : spec_(spec),
      init_pid_(kInvalidPid),
      state_(Container::State::STOPPED),
      desired_state_(Container::State::RUNNING),
      generation_(0) {
}

Container::~Container() {}

bool Container::Launch(GermZygote* zygote) {
  DCHECK_EQ(State::STOPPED, state_);
  ++generation_;

  if (!zygote->StartContainer(spec_, &init_pid_)) {
    return false;
  }

  state_ = State::RUNNING;
  return true;
}

bool Container::Terminate(GermZygote* zygote, base::TimeDelta kill_delay) {
  DCHECK_EQ(State::RUNNING, state_);
  if (!Kill(zygote, SIGTERM)) {
    return false;
  }

  // Note: zygote must outlive the message loop.
  CHECK(base::MessageLoop::current()->task_runner()->PostDelayedTask(
      FROM_HERE, base::Bind(&Container::SendSIGKILL, this, zygote, generation_),
      kill_delay));

  state_ = State::DYING;
  return true;
}

void Container::SendSIGKILL(GermZygote* zygote, uint64 generation) {
  if (generation != generation_ || state_ != State::DYING) {
    return;
  }

  Kill(zygote, SIGKILL);
}

bool Container::Kill(GermZygote* zygote, int signal) {
  CHECK_NE(kInvalidPid, init_pid_);
  DCHECK(state_ == State::RUNNING || state_ == State::DYING);
  return zygote->Kill(init_pid_, signal);
}

void Container::OnReap() {
  DCHECK(state_ == State::RUNNING || state_ == State::DYING);
  init_pid_ = kInvalidPid;
  state_ = State::STOPPED;
}

std::ostream& operator<<(std::ostream& os, Container::State state) {
  switch (state) {
    case Container::State::STOPPED:
      return os << "STOPPED";
    case Container::State::RUNNING:
      return os << "RUNNING";
    case Container::State::DYING:
      return os << "DYING";
  }
  NOTREACHED();
  return os << "[invalid state]";
}

}  // namespace germ
