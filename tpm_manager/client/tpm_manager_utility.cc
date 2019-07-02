// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tpm_manager/client/tpm_manager_utility.h"

#include <base/bind.h>
#include <base/logging.h>

namespace tpm_manager {

TpmManagerUtility::TpmManagerUtility(
    tpm_manager::TpmOwnershipInterface* tpm_owner,
    tpm_manager::TpmNvramInterface* tpm_nvram)
    : tpm_owner_(tpm_owner), tpm_nvram_(tpm_nvram) {}

bool TpmManagerUtility::Initialize() {
  if (!tpm_manager_thread_.IsRunning() &&
      !tpm_manager_thread_.StartWithOptions(base::Thread::Options(
          base::MessageLoopForIO::TYPE_IO, 0 /* Uses default stack size. */))) {
    LOG(ERROR) << "Failed to start tpm_manager thread.";
    return false;
  }
  if (!tpm_owner_ || !tpm_nvram_) {
    base::WaitableEvent event(base::WaitableEvent::ResetPolicy::MANUAL,
                              base::WaitableEvent::InitialState::NOT_SIGNALED);
    tpm_manager_thread_.task_runner()->PostTask(
        FROM_HERE, base::Bind(&TpmManagerUtility::InitializationTask,
                              base::Unretained(this), &event));
    event.Wait();
  }
  if (!tpm_owner_ || !tpm_nvram_) {
    LOG(ERROR) << "Failed to initialize tpm_managerd clients.";
    return false;
  }

  return true;
}

bool TpmManagerUtility::TakeOwnership() {
  tpm_manager::TakeOwnershipReply reply;
  tpm_manager::TakeOwnershipRequest request;
  SendTpmManagerRequestAndWait(
      base::Bind(&tpm_manager::TpmOwnershipInterface::TakeOwnership,
                 base::Unretained(tpm_owner_), request),
      &reply);
  if (reply.status() != tpm_manager::STATUS_SUCCESS) {
    LOG(ERROR) << __func__ << ": Failed to take ownership.";
    return false;
  }
  return true;
}

bool TpmManagerUtility::GetTpmStatus(bool* is_enabled,
                                     bool* is_owned,
                                     LocalData* local_data) {
  tpm_manager::GetTpmStatusReply tpm_status;
  SendTpmManagerRequestAndWait(
      base::Bind(&tpm_manager::TpmOwnershipInterface::GetTpmStatus,
                 base::Unretained(tpm_owner_),
                 tpm_manager::GetTpmStatusRequest()),
      &tpm_status);
  if (tpm_status.status() != tpm_manager::STATUS_SUCCESS) {
    LOG(ERROR) << __func__ << ": Failed to read TPM state from tpm_managerd.";
    return false;
  }
  *is_enabled = tpm_status.enabled();
  *is_owned = tpm_status.owned();
  tpm_status.mutable_local_data()->Swap(local_data);
  return true;
}

bool TpmManagerUtility::RemoveOwnerDependency(const std::string& dependency) {
  tpm_manager::RemoveOwnerDependencyReply reply;
  tpm_manager::RemoveOwnerDependencyRequest request;
  request.set_owner_dependency(dependency);
  SendTpmManagerRequestAndWait(
      base::Bind(&tpm_manager::TpmOwnershipInterface::RemoveOwnerDependency,
                 base::Unretained(tpm_owner_), request),
      &reply);
  if (reply.status() != tpm_manager::STATUS_SUCCESS) {
    LOG(ERROR) << __func__ << ": Failed to remove the dependency of "
               << dependency << ".";
    return false;
  }
  return true;
}

void TpmManagerUtility::InitializationTask(base::WaitableEvent* completion) {
  CHECK(completion);
  CHECK(tpm_manager_thread_.task_runner()->RunsTasksOnCurrentThread());

  default_tpm_owner_ = std::make_unique<tpm_manager::TpmOwnershipDBusProxy>();
  default_tpm_nvram_ = std::make_unique<tpm_manager::TpmNvramDBusProxy>();
  if (default_tpm_owner_->Initialize()) {
    default_tpm_owner_->ConnectToSignal(this);
    tpm_owner_ = default_tpm_owner_.get();
  }
  if (default_tpm_nvram_->Initialize()) {
    tpm_nvram_ = default_tpm_nvram_.get();
  }
  completion->Signal();
}

template <typename ReplyProtoType, typename MethodType>
void TpmManagerUtility::SendTpmManagerRequestAndWait(
    const MethodType& method, ReplyProtoType* reply_proto) {
  base::WaitableEvent event(base::WaitableEvent::ResetPolicy::MANUAL,
                            base::WaitableEvent::InitialState::NOT_SIGNALED);
  auto callback = base::Bind(
      [](ReplyProtoType* target, base::WaitableEvent* completion,
         const ReplyProtoType& reply) {
        *target = reply;
        completion->Signal();
      },
      reply_proto, &event);
  tpm_manager_thread_.task_runner()->PostTask(FROM_HERE,
                                              base::Bind(method, callback));
  event.Wait();
}

void TpmManagerUtility::ShutdownTask() {
  tpm_owner_ = nullptr;
  tpm_nvram_ = nullptr;
  default_tpm_owner_.reset(nullptr);
  default_tpm_nvram_.reset(nullptr);
}

bool TpmManagerUtility::GetDictionaryAttackInfo(int* counter,
                                                int* threshold,
                                                bool* lockout,
                                                int* seconds_remaining) {
  tpm_manager::GetDictionaryAttackInfoReply reply;
  tpm_manager::GetDictionaryAttackInfoRequest request;
  SendTpmManagerRequestAndWait(
      base::Bind(&tpm_manager::TpmOwnershipInterface::GetDictionaryAttackInfo,
                 base::Unretained(tpm_owner_), request),
      &reply);
  if (reply.status() != tpm_manager::STATUS_SUCCESS) {
    LOG(ERROR) << __func__
               << ": Failed to retreive the dictionary attack information.";
    return false;
  }

  *counter = reply.dictionary_attack_counter();
  *threshold = reply.dictionary_attack_threshold();
  *lockout = reply.dictionary_attack_lockout_in_effect();
  *seconds_remaining = reply.dictionary_attack_lockout_seconds_remaining();

  return true;
}

bool TpmManagerUtility::ResetDictionaryAttackLock() {
  tpm_manager::ResetDictionaryAttackLockReply reply;
  tpm_manager::ResetDictionaryAttackLockRequest request;
  SendTpmManagerRequestAndWait(
      base::Bind(&tpm_manager::TpmOwnershipInterface::ResetDictionaryAttackLock,
                 base::Unretained(tpm_owner_), request),
      &reply);
  if (reply.status() != tpm_manager::STATUS_SUCCESS) {
    LOG(ERROR) << __func__ << ": Failed to reset DA lock.";
    return false;
  }
  return true;
}

bool TpmManagerUtility::ReadSpace(uint32_t index,
                                  bool use_owner_auth,
                                  std::string* output) {
  // TODO(cylai): implement this function and test it once the server side
  // implementation is ready.
  DCHECK(false) << "Not implemented";
  return false;
}

bool TpmManagerUtility::GetOwnershipTakenSignalStatus(bool* is_successful,
                                                      bool* has_received,
                                                      LocalData* local_data) {
  base::AutoLock lock(ownership_signal_lock_);
  if (!is_connected_) {
    return false;
  }
  if (is_successful) {
    *is_successful = is_connection_successful_;
  }
  if (has_received) {
    *has_received = static_cast<bool>(ownership_taken_signal_);
  }
  // Copies |LocalData| when both the data source and destination is ready.
  if (ownership_taken_signal_ && local_data) {
    *local_data = ownership_taken_signal_->local_data();
  }
  return true;
}

void TpmManagerUtility::OnOwnershipTaken(const OwnershipTakenSignal& signal) {
  LOG(INFO) << __func__ << ": Received |OwnershipTakenSignal|.";
  base::AutoLock lock(ownership_signal_lock_);
  ownership_taken_signal_ = signal;
}

void TpmManagerUtility::OnSignalConnected(const std::string& /*interface_name*/,
                                          const std::string& /*signal_name*/,
                                          bool is_successful) {
  if (!is_successful) {
    LOG(ERROR) << __func__ << ": Failed to connect dbus signal.";
  } else {
    LOG(INFO) << __func__ << ": Connected dbus signal successfully.";
  }
  base::AutoLock lock(ownership_signal_lock_);
  is_connected_ = true;
  is_connection_successful_ = is_successful;
}

}  // namespace tpm_manager
