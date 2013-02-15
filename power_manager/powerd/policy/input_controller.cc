// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/powerd/policy/input_controller.h"

#include "chromeos/dbus/service_constants.h"
#include "power_manager/common/dbus_sender.h"
#include "power_manager/common/prefs.h"
#include "power_manager/common/util.h"
#include "power_manager/input_event.pb.h"
#include "power_manager/powerd/system/input.h"

namespace power_manager {
namespace policy {

InputController::InputController(system::Input* input,
                                 Delegate* delegate,
                                 DBusSenderInterface* dbus_sender,
                                 const base::FilePath& run_dir)
    : input_(input),
      delegate_(delegate),
      dbus_sender_(dbus_sender),
      lid_state_(LID_OPEN),
      use_input_for_lid_(true) {
  input_->AddObserver(this);
}

InputController::~InputController() {
  input_->RemoveObserver(this);
}

void InputController::Init(PrefsInterface* prefs) {
  CHECK(prefs->GetBool(kUseLidPref, &use_input_for_lid_));
  LidState lid_state = LID_OPEN;
  if (use_input_for_lid_ && input_->QueryLidState(&lid_state))
    OnLidEvent(lid_state);
}

void InputController::OnLidEvent(LidState state) {
  LOG(INFO) << "Lid " << LidStateToString(state);
  lid_state_ = state;
  if (!use_input_for_lid_) {
    LOG(WARNING) << "Ignoring lid";
    return;
  }

  InputEvent proto;
  if (lid_state_ == LID_CLOSED) {
    input_->SetTouchDevicesState(false);
    input_->SetWakeInputsState(false);
    delegate_->HandleLidClosed();
    proto.set_type(InputEvent_Type_LID_CLOSED);
  } else {
    input_->SetTouchDevicesState(true);
    input_->SetWakeInputsState(true);
    delegate_->HandleLidOpened();
    proto.set_type(InputEvent_Type_LID_OPEN);
  }
  proto.set_timestamp(base::TimeTicks::Now().ToInternalValue());
  dbus_sender_->EmitSignalWithProtocolBuffer(kInputEventSignal, proto);
}

void InputController::OnPowerButtonEvent(ButtonState state) {
  if (state != BUTTON_REPEAT) {
    InputEvent proto;
    proto.set_type(state == BUTTON_DOWN ?
                   InputEvent_Type_POWER_BUTTON_DOWN :
                   InputEvent_Type_POWER_BUTTON_UP);
    proto.set_timestamp(base::TimeTicks::Now().ToInternalValue());
    dbus_sender_->EmitSignalWithProtocolBuffer(kInputEventSignal, proto);
  }

  delegate_->SendPowerButtonMetric(state == BUTTON_DOWN,
                                   base::TimeTicks::Now());

  if (state == BUTTON_DOWN) {
#ifndef IS_DESKTOP
    delegate_->EnsureBacklightIsOn();
#endif
    LOG(INFO) << "Syncing state due to power button down event";
    util::Launch("sync");
  }
}

}  // namespace policy
}  // namespace power_manager
