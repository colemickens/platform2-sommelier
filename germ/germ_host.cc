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
#include <base/time/time.h>

#include "germ/container.h"
#include "germ/container_manager.h"
#include "germ/proto_bindings/soma_container_spec.pb.h"

namespace germ {

GermHost::GermHost(GermZygote* zygote)
    : zygote_(zygote), container_manager_(zygote) {}

Status GermHost::Launch(LaunchRequest* request, LaunchResponse* response) {
  const soma::ContainerSpec& spec = request->spec();
  soma::ReadOnlyContainerSpec ro_spec;
  if (!ro_spec.Init(spec)) {
    return STATUS_APP_ERROR_LOG(logging::LOG_ERROR,
                                LaunchResponse::INVALID_SPEC,
                                "Could not initialize read-only ContainerSpec");
  }

  if (!container_manager_.StartContainer(spec)) {
    return STATUS_APP_ERROR_LOG(logging::LOG_ERROR,
                                LaunchResponse::START_CONTAINER_FAILED,
                                "Failed to start container: " + spec.name());
  }

  return STATUS_OK();
}

Status GermHost::Terminate(TerminateRequest* request,
                           TerminateResponse* response) {
  // Temporarily disabled, the implementation will be added in a subsequent CL.
  return STATUS_APP_ERROR_LOG(logging::LOG_ERROR,
                              TerminateResponse::TERMINATE_FAILED,
                              "Not implemented");
}

void GermHost::HandleReapedChild(const siginfo_t& info) {
  const pid_t pid = info.si_pid;
  LOG(INFO) << "GermHost: Process " << pid << " terminated with status "
            << info.si_status << " (code = " << info.si_code << ")";

  if (pid == zygote_->pid()) {
    LOG(FATAL) << "Zygote died!";
  }

  container_manager_.OnReap(info);
}

}  // namespace germ
