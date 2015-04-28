// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GERM_GERM_ZYGOTE_H_
#define GERM_GERM_ZYGOTE_H_

#include <sys/types.h>

#include <base/files/scoped_file.h>
#include <base/macros.h>
#include <chromeos/daemons/daemon.h>

#include "germ/process_reaper.h"
#include "germ/proto_bindings/soma_container_spec.pb.h"

namespace germ {

// Implements a zygote process that container init processes are forked off of.
// This zygote process should be started with Start() before any binder channels
// are established. The calling process then becomes the parent of the zygote
// process. StartContainer() can then be called from the parent to start up
// containers. This class releases ownership of container processes by double
// forking. In order to take responsibility for reaping these children, the
// parent may call prctl(PR_SET_CHILD_SUBREAPER, 1) before starting the zygote
// to take ownership of these processes.
class GermZygote {
 public:
  GermZygote();
  virtual ~GermZygote();

  // Forks off a zygote process which listens for requests on a unix socket.
  void Start();

  // Makes a request to the zygote process to spawn a container. Run from the
  // zygote's parent. Returns false on failure. On success, |pid| is populated
  // with the pid of the container's init process.
  bool StartContainer(const soma::ContainerSpec& spec, pid_t* pid);

  pid_t pid() const { return zygote_pid_; }

 private:
  // Fork off the container init process. On success, returns the init process's
  // pid in the parent and 0 in the child. Returns -1 on error. Should only be
  // overriden in tests.
  virtual pid_t ForkContainer(const soma::ContainerSpec& spec);

  // Zygote process request loop. Does not return.
  void HandleRequests();

  // Runs from the zygote process.
  void SpawnContainer(const soma::ContainerSpec& spec, int client_fd);

  pid_t zygote_pid_;

  // Unix socket used to send requests to the zygote procses.
  base::ScopedFD client_fd_;
  base::ScopedFD server_fd_;

  DISALLOW_COPY_AND_ASSIGN(GermZygote);
};

}  // namespace germ

#endif  // GERM_GERM_ZYGOTE_H_
