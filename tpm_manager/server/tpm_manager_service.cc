// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tpm_manager/server/tpm_manager_service.h"

#include <base/callback.h>
#include <chromeos/bind_lambda.h>

namespace tpm_manager {

TpmManagerService::TpmManagerService() : weak_factory_(this) {}

bool TpmManagerService::Initialize() {
  LOG(INFO) << "TpmManager service started.";
  worker_thread_.reset(new base::Thread("TpmManager Service Worker"));
  worker_thread_->StartWithOptions(
      base::Thread::Options(base::MessageLoop::TYPE_IO, 0));
  return true;
}

void TpmManagerService::GetTpmStatus(const GetTpmStatusRequest& request,
                                     const GetTpmStatusCallback& callback) {
  auto result = std::make_shared<GetTpmStatusReply>();
  base::Closure task = base::Bind(&TpmManagerService::GetTpmStatusTask,
                                  base::Unretained(this), request, result);
  base::Closure reply = base::Bind(
      &TpmManagerService::TaskRelayCallback<GetTpmStatusReply>,
      weak_factory_.GetWeakPtr(),
      callback,
      result);
  worker_thread_->task_runner()->PostTaskAndReply(FROM_HERE, task, reply);
}

void TpmManagerService::GetTpmStatusTask(
    const GetTpmStatusRequest& request,
    const std::shared_ptr<GetTpmStatusReply>& result) {
  VLOG(1) << "Performing GetTpmStatusTask.";
  result->set_status(STATUS_NOT_AVAILABLE);
}

}  // namespace tpm_manager
