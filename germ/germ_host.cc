// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "germ/germ_host.h"

#include <string>
#include <vector>

#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <base/logging.h>
#include <base/posix/eintr_wrapper.h>
#include <soma/read_only_container_spec.h>

#include "germ/proto_bindings/soma_container_spec.pb.h"

namespace germ {

GermHost::GermHost(GermZygote* zygote) : zygote_(zygote) {}

Status GermHost::Launch(LaunchRequest* request, LaunchResponse* response) {
  soma::ReadOnlyContainerSpec ro_spec;
  if (!ro_spec.Init(request->spec())) {
    return STATUS_APP_ERROR_LOG(logging::LOG_ERROR,
                                LaunchResponse::INVALID_SPEC,
                                "Could not initialize read-only ContainerSpec");
  }

  pid_t pid = -1;
  if (!zygote_->StartContainer(request->spec(), &pid)) {
    return STATUS_APP_ERROR_LOG(
        logging::LOG_ERROR, LaunchResponse::START_CONTAINER_FAILED,
        "StartContainer(" + request->name() + ") failed");
  }

  response->set_pid(pid);
  return STATUS_OK();
}

Status GermHost::Terminate(TerminateRequest* request,
                           TerminateResponse* response) {
  // TODO(rickyz): Make this take a container name instead.
  const pid_t pid = request->pid();

  if (kill(pid, SIGTERM) != 0) {
    return STATUS_APP_ERROR_LOG(logging::LOG_ERROR,
                                TerminateResponse::TERMINATE_FAILED,
                                "kill(" + std::to_string(pid) + ") failed");
  }

  const pid_t rc = HANDLE_EINTR(waitpid(pid, nullptr, 0));
  if (rc != pid) {
    return STATUS_APP_ERROR_LOG(
        logging::LOG_ERROR, TerminateResponse::TERMINATE_FAILED,
        "waitpid(" + std::to_string(pid) + ", ...) failed");
  }

  return STATUS_OK();
}

}  // namespace germ
