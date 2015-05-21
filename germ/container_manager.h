// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GERM_CONTAINER_MANAGER_H_
#define GERM_CONTAINER_MANAGER_H_

#include <sys/types.h>
#include <sys/wait.h>

#include <map>
#include <memory>
#include <string>

#include <base/macros.h>
#include <base/memory/ref_counted.h>

#include "germ/container.h"
#include "germ/germ_zygote.h"
#include "germ/proto_bindings/soma_sandbox_spec.pb.h"

namespace germ {

// Manages the lifetime of Germ containers. This class sets the desired state
// for each container and performs the appropriate operations to move containers
// into their desired state.
class ContainerManager {
 public:
  explicit ContainerManager(GermZygote* zygote);
  ~ContainerManager();

  // Starts a container. If a container with the given name is already running,
  // its spec is set to |spec| and the container is restarted. Returns true on
  // success.
  bool StartContainer(const soma::SandboxSpec& spec);

  // Terminates a container. When the container's init process is reaped, the
  // container object will be removed from the ContainerManager entirely.
  // Returns true on success.
  bool TerminateContainer(const std::string& name);

  // Called when a container init process has been reaped.
  void OnReap(const siginfo_t& info);

  // Lookup a container with the given name. Returns nullptr if not found.
  scoped_refptr<Container> Lookup(const std::string& name);

 private:
  // Terminate the container. Depending on the container's desired state, the
  // container may be automatically restarted after it dies.  Returns true on
  // success.
  bool DoTerminate(scoped_refptr<Container> container);

  // Starts a container and adds an entry for its init process into |pid_map_|.
  // Does *not* add the container to |containers_|. Returns true on success.
  bool DoStart(scoped_refptr<Container> container);

  // name -> container
  std::map<std::string, scoped_refptr<Container>> containers_;

  // init pid -> container
  std::map<pid_t, scoped_refptr<Container>> pid_map_;

  // Zygote used for launching containers.
  GermZygote* zygote_;

  DISALLOW_COPY_AND_ASSIGN(ContainerManager);
};

}  // namespace germ

#endif  // GERM_CONTAINER_MANAGER_H_
