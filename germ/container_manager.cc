// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "germ/container_manager.h"

#include <sys/types.h>
#include <sys/wait.h>

#include <map>
#include <memory>
#include <string>

#include <base/logging.h>
#include <base/memory/ref_counted.h>
#include <base/stl_util.h>
#include <base/time/time.h>

#include "germ/container.h"
#include "germ/proto_bindings/soma_container_spec.pb.h"

namespace germ {

ContainerManager::ContainerManager(GermZygote* zygote) : zygote_(zygote) {}

ContainerManager::~ContainerManager() {}

bool ContainerManager::StartContainer(const soma::ContainerSpec& spec) {
  const std::string& name = spec.name();
  scoped_refptr<Container> container = Lookup(name);
  if (container == nullptr) {
    container = new Container(spec);
    containers_[name] = container;
    return DoStart(container);
  }

  container->set_spec(spec);
  container->set_desired_state(Container::State::RUNNING);

  switch (container->state()) {
    case Container::State::STOPPED:
      return DoStart(container);

    case Container::State::RUNNING:
      // TODO(rickyz): Make kill_delay part of the ContainerSpec.
      return DoTerminate(container, base::TimeDelta());

    case Container::State::DYING:
      // TODO(rickyz): This return value sucks.
      return true;
  }

  NOTREACHED();
  return false;
}

bool ContainerManager::TerminateContainer(const std::string& name,
                                          base::TimeDelta kill_delay) {
  scoped_refptr<Container> container = Lookup(name);
  if (container == nullptr) {
    LOG(ERROR) << "Attempted to terminate nonexistent container: " << name;
    return false;
  }

  container->set_desired_state(Container::State::STOPPED);
  return DoTerminate(container, kill_delay);
}

bool ContainerManager::DoStart(scoped_refptr<Container> container) {
  DCHECK_EQ(Container::State::RUNNING, container->desired_state());
  DCHECK_EQ(Container::State::STOPPED, container->state());

  if (!container->Launch(zygote_)) {
    return false;
  }

  pid_map_[container->init_pid()] = container;
  return true;
}

bool ContainerManager::DoTerminate(scoped_refptr<Container> container,
                                   base::TimeDelta kill_delay) {
  switch (container->state()) {
    case Container::State::STOPPED:
      return true;

    case Container::State::RUNNING:
      return container->Terminate(zygote_, kill_delay);

    case Container::State::DYING:
      return true;
  }

  NOTREACHED();
  return false;
}

scoped_refptr<Container> ContainerManager::Lookup(const std::string& name) {
  const auto it = containers_.find(name);
  if (it == containers_.end()) {
    return nullptr;
  }

  return it->second;
}

void ContainerManager::OnReap(const siginfo_t& info) {
  const pid_t pid = info.si_pid;
  const auto it = pid_map_.find(pid);
  if (it == pid_map_.end()) {
    LOG(ERROR) << "Received SIGCHLD from unknown process: " << pid;
    return;
  }

  scoped_refptr<Container> container = it->second;
  const std::string& name = container->name();
  pid_map_.erase(it);
  container->OnReap();

  switch (container->desired_state()) {
    case Container::State::STOPPED:
      CHECK_EQ(1u, containers_.erase(name));
      return;

    case Container::State::RUNNING:
      if (!DoStart(container)) {
        LOG(ERROR) << "Failed to restart container: " << name;
        // TODO(rickyz): Should we queue up any further attempts to restart the
        // container?
        CHECK_EQ(1u, containers_.erase(name));
      }
      return;

    case Container::State::DYING:
      DLOG(FATAL) << "Invalid desired state for container " << name << ": "
                  << container->desired_state();
      return;
  }

  NOTREACHED();
}

}  // namespace germ

