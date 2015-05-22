// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GERM_CONTAINER_H_
#define GERM_CONTAINER_H_

#include <sys/types.h>

#include <ostream>
#include <string>

#include <base/basictypes.h>
#include <base/macros.h>
#include <base/memory/ref_counted.h>
#include <base/time/time.h>

#include "germ/germ_zygote.h"
#include "germ/proto_bindings/soma_sandbox_spec.pb.h"

namespace germ {

// Class representing a Germ container. A container may be in the following
// states:
//
// STOPPED: The container is stopped. This is a container's initial state.
// RUNNING: The container is running.
// DYING: The container has been killed, but it has not been reaped yet (so it
// is still associated with a PID). Once the container has been reaped, it
// transitions to STOPPED.
//
// A container's life cycle normally something like this:
//
//          Launch            Terminate          Reap
// STOPPED --------> RUNNING -----------> DYING ------> STOPPED
//
// A container may also go directly from RUNNING to STOPPED if it terminates on
// its own (without Terminate being called on it).
class Container : public base::RefCounted<Container> {
 public:
  enum class State {
    STOPPED,
    RUNNING,
    DYING,
  };

  explicit Container(const soma::SandboxSpec& spec);
  ~Container();

  const soma::SandboxSpec& spec() const { return spec_; }
  const std::string& name() const { return spec_.name(); }
  pid_t init_pid() const { return init_pid_; }
  State state() const { return state_; }
  State desired_state() const { return desired_state_; }

  void set_desired_state(State desired_state) {
    DCHECK(desired_state == State::STOPPED || desired_state == State::RUNNING);
    desired_state_ = desired_state;
  }

  void set_spec(const soma::SandboxSpec& spec) {
    DCHECK_EQ(name(), spec.name());
    spec_ = spec;
  }

  // Launches the container. The container must be STOPPED. On success, returns
  // true and transitions the container to RUNNING.
  bool Launch(GermZygote* launcher);

  // Terminates the container by sending SIGTERM. If the container does not die
  // after |kill_delay|, we send SIGKILL. The container must be RUNNING. On
  // success, returns true and transitions the container to DYING.
  bool Terminate(GermZygote* zygote, base::TimeDelta kill_delay);

  // Called when container init has been reaped. The container should be RUNNING
  // or DYING. Transitions the container to STOPPED.
  void OnReap();

  // Send a signal to the container's init pid. The container must be either
  // RUNNING or DYING. Returns true on success.
  bool Kill(GermZygote* zygote, int signal);

 private:
  // Callback which sends SIGKILL to the container init process.
  void SendSIGKILL(GermZygote* zygote, uint64 generation);

  soma::SandboxSpec spec_;

  pid_t init_pid_;

  State state_;

  // The desired state. ContainerManager is responsible for setting this and
  // performing the necessary actions to bring the container into the desired
  // state.
  State desired_state_;

  // When a container is terminated we send SIGTERM and schedule a SIGKILL to be
  // sent after some delay. In the interim, the container may have been
  // successfully terminated and relaunched. To avoid this issue, we keep a
  // generation counter that is incremented each time the container is launched.
  // Then in the SIGKILL callback, we return immediately if the container has a
  // different generation than at the time the SIGKILL was scheduled.
  uint64 generation_;

  friend class TestContainer;

  DISALLOW_COPY_AND_ASSIGN(Container);
};

std::ostream& operator<<(std::ostream& os, Container::State state);

}  // namespace germ

#endif  // GERM_CONTAINER_H_
