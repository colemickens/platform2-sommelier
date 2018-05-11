// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bluetooth/newblued/newblue.h"

#include <memory>
#include <utility>

#include <base/bind.h>
#include <base/message_loop/message_loop.h>

namespace bluetooth {

Newblue::Newblue(std::unique_ptr<LibNewblue> libnewblue)
    : libnewblue_(std::move(libnewblue)), weak_ptr_factory_(this) {}

base::WeakPtr<Newblue> Newblue::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

bool Newblue::Init() {
  if (base::MessageLoop::current())
    origin_task_runner_ = base::MessageLoop::current()->task_runner();
  return true;
}

void Newblue::ListenReadyForUp(base::Closure callback) {
  // Dummy MAC address. NewBlue doesn't actually use the MAC address as it's
  // exclusively controlled by BlueZ.
  static const uint8_t kZeroMac[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

  ready_for_up_callback_ = callback;
  libnewblue_->HciUp(kZeroMac, &Newblue::OnStackReadyForUpThunk, this);
}

bool Newblue::BringUp() {
  if (!libnewblue_->HciIsUp()) {
    LOG(ERROR) << "HCI is not ready for up";
    return false;
  }

  if (libnewblue_->L2cInit()) {
    LOG(ERROR) << "Failed to initialize L2CAP";
    return false;
  }

  if (!libnewblue_->AttInit()) {
    LOG(ERROR) << "Failed to initialize ATT";
    return false;
  }

  if (!libnewblue_->GattProfileInit()) {
    LOG(ERROR) << "Failed to initialize GATT";
    return false;
  }

  if (!libnewblue_->GattBuiltinInit()) {
    LOG(ERROR) << "Failed to initialize GATT services";
    return false;
  }

  if (!libnewblue_->SmInit(HCI_DISP_CAP_NONE)) {
    LOG(ERROR) << "Failed to init SM";
    return false;
  }

  return true;
}

bool Newblue::PostTask(const tracked_objects::Location& from_here,
                       const base::Closure& task) {
  CHECK(origin_task_runner_.get());
  return origin_task_runner_->PostTask(from_here, task);
}

void Newblue::OnStackReadyForUpThunk(void* data) {
  Newblue* newblue = static_cast<Newblue*>(data);
  newblue->PostTask(FROM_HERE, base::Bind(&Newblue::OnStackReadyForUp,
                                          newblue->GetWeakPtr()));
}

void Newblue::OnStackReadyForUp() {
  if (!ready_for_up_callback_.is_null())
    ready_for_up_callback_.Run();
}

}  // namespace bluetooth
