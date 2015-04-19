// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "germ/germ_host.h"

#include <string>
#include <vector>

#include <base/logging.h>
#include <soma/read_only_container_spec.h>

namespace germ {

Status GermHost::Launch(LaunchRequest* request, LaunchResponse* response) {
  pid_t pid = -1;
  soma::ReadOnlyContainerSpec ro_spec;
  if (!ro_spec.Init(request->spec())) {
    return STATUS_APP_ERROR_LOG(logging::LOG_ERROR,
                                LaunchResponse::INIT_SPEC_FAILED,
                                "Could not initialize read-only ContainerSpec");
  }
  if (!launcher_.RunDaemonized(ro_spec, &pid)) {
    return STATUS_APP_ERROR_LOG(
        logging::LOG_ERROR, LaunchResponse::RUN_DAEMONIZED_FAILED,
        "RunDaemonized(" + request->name() + ") failed");
  }
  response->set_pid(pid);
  return STATUS_OK();
}

Status GermHost::Terminate(TerminateRequest* request,
                           TerminateResponse* response) {
  bool success = launcher_.Terminate(request->pid());
  if (!success) {
    return STATUS_APP_ERROR_LOG(
        logging::LOG_ERROR, TerminateResponse::TERMINATE_FAILED,
        "Terminate(" + std::to_string(request->pid()) + ") failed");
  }
  return STATUS_OK();
}

}  // namespace germ
