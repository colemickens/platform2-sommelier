// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tpm_manager/server/tpm_manager_service.h"

#include <base/callback.h>
#include <base/command_line.h>
#include <chromeos/bind_lambda.h>

namespace tpm_manager {

TpmManagerService::TpmManagerService(bool wait_for_ownership,
                                     LocalDataStore* local_data_store,
                                     TpmStatus* tpm_status,
                                     TpmInitializer* tpm_initializer)
    : local_data_store_(local_data_store),
      tpm_status_(tpm_status),
      tpm_initializer_(tpm_initializer),
      wait_for_ownership_(wait_for_ownership),
      weak_factory_(this) {
}

bool TpmManagerService::Initialize() {
  LOG(INFO) << "TpmManager service started.";
  worker_thread_.reset(new base::Thread("TpmManager Service Worker"));
  worker_thread_->StartWithOptions(
      base::Thread::Options(base::MessageLoop::TYPE_IO, 0));
  base::Closure task = base::Bind(&TpmManagerService::InitializeTask,
                                  base::Unretained(this));
  worker_thread_->task_runner()->PostNonNestableTask(FROM_HERE, task);
  return true;
}

void TpmManagerService::InitializeTask() {
  if (!tpm_status_->IsTpmEnabled()) {
    LOG(WARNING) << __func__ << ": TPM is disabled.";
    return;
  }
  if (!wait_for_ownership_) {
    VLOG(1) << "Initializing TPM.";
    if (!tpm_initializer_->InitializeTpm()) {
      LOG(WARNING) << __func__ << ": TPM initialization failed.";
      return;
    }
  }
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
  VLOG(1) << __func__;
  result->set_enabled(tpm_status_->IsTpmEnabled());
  result->set_owned(tpm_status_->IsTpmOwned());
  LocalData local_data;
  if (local_data_store_ && local_data_store_->Read(&local_data)) {
    *result->mutable_local_data() = local_data;
  }
  int counter;
  int threshold;
  bool lockout;
  int lockout_time_remaining;
  if (tpm_status_->GetDictionaryAttackInfo(&counter, &threshold, &lockout,
                                           &lockout_time_remaining)) {
    result->set_dictionary_attack_counter(counter);
    result->set_dictionary_attack_threshold(threshold);
    result->set_dictionary_attack_lockout_in_effect(lockout);
    result->set_dictionary_attack_lockout_seconds_remaining(
        lockout_time_remaining);
  }
}

void TpmManagerService::TakeOwnership(const TakeOwnershipRequest& request,
                                      const TakeOwnershipCallback& callback) {
  auto result = std::make_shared<TakeOwnershipReply>();
  base::Closure task = base::Bind(&TpmManagerService::TakeOwnershipTask,
                                  base::Unretained(this), request, result);
  base::Closure reply = base::Bind(
      &TpmManagerService::TaskRelayCallback<TakeOwnershipReply>,
      weak_factory_.GetWeakPtr(),
      callback,
      result);
  worker_thread_->task_runner()->PostTaskAndReply(FROM_HERE, task, reply);
}

void TpmManagerService::TakeOwnershipTask(
    const TakeOwnershipRequest& request,
    const std::shared_ptr<TakeOwnershipReply>& result) {
  VLOG(1) << __func__;
  if (!tpm_status_->IsTpmEnabled()) {
    result->set_status(STATUS_NOT_AVAILABLE);
    return;
  }
  if (!tpm_initializer_->InitializeTpm()) {
    result->set_status(STATUS_UNEXPECTED_DEVICE_ERROR);
    return;
  }
  result->set_status(STATUS_SUCCESS);
}

}  // namespace tpm_manager
