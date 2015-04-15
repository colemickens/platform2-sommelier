// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "germ/germ_host.h"

#include <string>
#include <vector>

#include <base/logging.h>
#include <soma/read_only_container_spec.h>

namespace germ {

int GermHost::Launch(LaunchRequest* request, LaunchResponse* response) {
  pid_t pid = -1;
  soma::ReadOnlyContainerSpec ro_spec;
  if (!ro_spec.Init(request->spec())) {
    // TODO(jorgelo): Unify error handling, either return value or |success|.
    LOG(ERROR) << "Could not initialize read-only ContainerSpec";
    response->set_success(false);
    response->set_pid(-1);
    return -1;
  }
  if (!launcher_.RunDaemonized(ro_spec, &pid)) {
    // TODO(jorgelo): Unify error handling, either return value or |success|.
    LOG(ERROR) << "RunDaemonized(" << request->name() << ") failed";
    response->set_success(false);
    response->set_pid(-1);
    return -1;
  }
  response->set_success(true);
  response->set_pid(pid);
  return 0;
}

int GermHost::Terminate(TerminateRequest* request,
                        TerminateResponse* response) {
  bool success = launcher_.Terminate(request->pid());
  if (!success) {
    // TODO(jorgelo): Unify error handling, either return value or |success|.
    LOG(ERROR) << "Terminate(" << request->pid() << ") failed";
    response->set_success(false);
    return -1;
  }
  response->set_success(success);
  return 0;
}

}  // namespace germ
