// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mount_task.h"

namespace cryptohome {

const char* kMountTaskResultEventType = "MountTaskResult";
const char* kPkcs11InitResultEventType = "Pkcs11InitResult";

base::AtomicSequenceNumber MountTask::sequence_holder_;

MountTask::MountTask(MountTaskObserver* observer,
                     Mount* mount,
                     const UsernamePasskey& credentials)
    : mount_(mount),
      credentials_(),
      sequence_id_(-1),
      observer_(observer),
      default_result_(new MountTaskResult),
      result_(default_result_.get()),
      complete_event_(NULL) {
  credentials_.Assign(credentials);
  sequence_id_ = NextSequence();
  result_->set_sequence_id(sequence_id_);
}

MountTask::~MountTask() {
}

int MountTask::NextSequence() {
  // AtomicSequenceNumber is zero-based, so increment so that the sequence ids
  // are one-based.
  return sequence_holder_.GetNext() + 1;
}

void MountTask::Notify() {
  if (observer_) {
    observer_->MountTaskObserve(*result_);
  }
  Signal();
}

void MountTask::Signal()
{
  if (complete_event_) {
    complete_event_->Signal();
  }
}

void MountTaskMount::Run() {
  if (mount_) {
    Mount::MountError code = Mount::MOUNT_ERROR_NONE;
    bool status = mount_->MountCryptohome(credentials_,
                                          mount_args_,
                                          &code);
    result()->set_return_status(status);
    result()->set_return_code(code);
  }
  MountTask::Notify();

  // Update user activity timestamp to be able to detect old users.
  // This action is not mandatory, so we perform it after
  // CryptohomeMount() returns, in background.
  if (mount_)
    mount_->UpdateUserActivityTimestamp();
}

void MountTaskMountGuest::Run() {
  if (mount_) {
    bool status = mount_->MountGuestCryptohome();
    result()->set_return_status(status);
  }
  MountTask::Notify();
}

void MountTaskMigratePasskey::Run() {
  if (mount_) {
    bool status = mount_->MigratePasskey(
        credentials_,
        static_cast<const char*>(old_key_.const_data()));
    result()->set_return_status(status);
  }
  MountTask::Notify();
}

void MountTaskUnmount::Run() {
  if (mount_) {
    bool status = mount_->UnmountCryptohome();
    result()->set_return_status(status);
  }
  MountTask::Notify();
}

void MountTaskTestCredentials::Run() {
  if (mount_) {
    bool status = mount_->TestCredentials(credentials_);
    result()->set_return_status(status);
  }
  MountTask::Notify();
}

void MountTaskRemove::Run() {
  if (mount_) {
    bool status = mount_->RemoveCryptohome(credentials_);
    result()->set_return_status(status);
  }
  MountTask::Notify();
}

void MountTaskResetTpmContext::Run() {
  if (mount_) {
    Crypto* crypto = mount_->get_crypto();
    if (crypto) {
      crypto->EnsureTpm(true);
    }
  }
  MountTask::Notify();
}

void MountTaskRemoveTrackedSubdirectories::Run() {
  result()->set_return_status(true);
  if (mount_) {
    mount_->CleanUnmountedTrackedSubdirectories();
  }
  MountTask::Notify();
}

void MountTaskAutomaticFreeDiskSpace::Run() {
  result()->set_return_status(true);
  if (mount_) {
    mount_->DoAutomaticFreeDiskSpaceControl();
  }
  MountTask::Notify();
}

MountTaskPkcs11Init::MountTaskPkcs11Init(MountTaskObserver* observer,
                                         Mount* mount)
    : MountTask(observer, mount, UsernamePasskey()),
      pkcs11_init_result_(new MountTaskResult(kPkcs11InitResultEventType)) {
  set_result(pkcs11_init_result_.get());
}

void MountTaskPkcs11Init::Run() {
  if (mount_) {
    // Initialize() determines if initialization is needed, and if
    // so, performs it.
    bool status = pkcs11_init_.Initialize();
    result()->set_return_status(status);
  }
  MountTask::Notify();
}

}  // namespace cryptohome
