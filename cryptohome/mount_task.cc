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

void MountTaskResetTpmContext::Run() {
  if (mount_) {
    Crypto* crypto = mount_->crypto();
    if (crypto) {
      crypto->EnsureTpm(true);
    }
  }
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

}  // namespace cryptohome
