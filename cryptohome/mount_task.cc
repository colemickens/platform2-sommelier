// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cryptohome/mount_task.h"

#include "cryptohome/install_attributes.h"

namespace cryptohome {

const char* kMountTaskResultEventType = "MountTaskResult";
const char* kPkcs11InitResultEventType = "Pkcs11InitResult";

MountTask::MountTask(MountTaskObserver* observer,
                     Mount* mount,
                     const UsernamePasskey& credentials,
                     int sequence_id)
    : mount_(mount),
      credentials_(),
      sequence_id_(sequence_id),
      cancel_flag_(false),
      observer_(observer),
      default_result_(new MountTaskResult),
      result_(default_result_.get()),
      complete_event_(NULL) {
  credentials_.Assign(credentials);
  result_->set_sequence_id(sequence_id_);
}

MountTask::MountTask(MountTaskObserver* observer, Mount* mount, int sequence_id)
    : mount_(mount),
      credentials_(),
      sequence_id_(sequence_id),
      cancel_flag_(false),
      observer_(observer),
      default_result_(new MountTaskResult),
      result_(default_result_.get()),
      complete_event_(NULL) {
  result_->set_sequence_id(sequence_id_);
}

MountTask::~MountTask() {
}

void MountTask::Notify() {
  if (observer_) {
    if (observer_->MountTaskObserve(*result_)) {
      delete observer_;
      observer_ = NULL;
    }
  }
  Signal();
}

void MountTask::Signal() {
  if (complete_event_) {
    complete_event_->Signal();
  }
}

void MountTaskMount::Run() {
  if (mount_) {
    MountError code = MOUNT_ERROR_NONE;
    bool status = mount_->MountCryptohome(credentials_,
                                          mount_args_,
                                          &code);
    // Ensure result() is not mismatched.
    result()->set_mount(mount_);
    result()->set_return_status(status);
    result()->set_return_code(code);
  }
  MountTask::Notify();

  // Update user activity timestamp to be able to detect old users.
  // This action is not mandatory, so we perform it after
  // CryptohomeMount() returns, in background.
  if (mount_)
    mount_->UpdateCurrentUserActivityTimestamp(0);
}

void MountTaskMountGuest::Run() {
  if (mount_) {
    bool status = mount_->MountGuestCryptohome();
    result()->set_return_status(status);
  }
  MountTask::Notify();
}

void MountTaskMigratePasskey::Run() {
  CHECK(homedirs_);
  bool status = homedirs_->Migrate(credentials_, old_key_);
  result()->set_return_status(status);
  MountTask::Notify();
}

void MountTaskUnmount::Run() {
  if (mount_) {
    bool status = mount_->UnmountCryptohome();
    result()->set_return_status(status);
  }
  MountTask::Notify();
}

void MountTaskRemove::Run() {
  if (homedirs_) {
    bool status = homedirs_->Remove(credentials_.username());
    result()->set_return_status(status);
  }
  MountTask::Notify();
}

void MountTaskResetTpmContext::Run() {
  if (mount_) {
    Crypto* crypto = mount_->crypto();
    if (crypto) {
      crypto->EnsureTpm(true);
    }
  }
  MountTask::Notify();
}

void MountTaskAutomaticFreeDiskSpace::Run() {
  bool rc = false;
  if (homedirs_) {
    homedirs_->FreeDiskSpace();
    rc = homedirs_->HasTargetFreeSpace();
  }
  result()->set_return_status(rc);
  MountTask::Notify();
}

void MountTaskUpdateCurrentUserActivityTimestamp::Run() {
  bool rc = false;
  if (mount_) {
    rc = mount_->UpdateCurrentUserActivityTimestamp(time_shift_sec_);
  }
  result()->set_return_status(rc);
  MountTask::Notify();
}

MountTaskPkcs11Init::MountTaskPkcs11Init(MountTaskObserver* observer,
                                         Mount* mount,
                                         int sequence_id)
    : MountTask(observer, mount, sequence_id),
      pkcs11_init_result_(new MountTaskResult(kPkcs11InitResultEventType)) {
  set_result(pkcs11_init_result_.get());
}

void MountTaskPkcs11Init::Run() {
  if (!IsCanceled() && mount_) {
    // This will send an insertion event to the Chaps daemon with appropriate
    // authorization data.
    if (mount_->IsMounted())
      mount_->InsertPkcs11Token();
    result()->set_return_status(1);
  }
  MountTask::Notify();
}

void MountTaskInstallAttrsFinalize::Run() {
  install_attrs_->Finalize();
}

}  // namespace cryptohome
