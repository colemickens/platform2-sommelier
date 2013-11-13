// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/powerd/policy/input_controller.h"

#include <glib.h>

#include "base/logging.h"
#include "chromeos/dbus/service_constants.h"
#include "power_manager/common/dbus_sender.h"
#include "power_manager/common/util.h"
#include "power_manager/input_event.pb.h"
#include "power_manager/powerd/system/input_interface.h"

namespace power_manager {
namespace policy {

namespace {

// Frequency with which CheckActiveVT() should be called, in seconds.
// This just needs to be lower than the screen-dimming delay.
const int kCheckActiveVTFrequencySec = 60;

} // namespace

InputController::InputController(system::InputInterface* input,
                                 Delegate* delegate,
                                 DBusSenderInterface* dbus_sender)
    : input_(input),
      delegate_(delegate),
      dbus_sender_(dbus_sender),
      lid_state_(LID_NOT_PRESENT),
      check_active_vt_timeout_id_(0) {
  input_->AddObserver(this);
}

InputController::~InputController() {
  input_->RemoveObserver(this);
  util::RemoveTimeout(&check_active_vt_timeout_id_);
}

void InputController::Init() {
  OnLidEvent(input_->QueryLidState());
  check_active_vt_timeout_id_ = g_timeout_add(
      kCheckActiveVTFrequencySec * 1000, CheckActiveVTThunk, this);
}

bool InputController::TriggerCheckActiveVTTimeoutForTesting() {
  if (!check_active_vt_timeout_id_)
    return false;

  return CheckActiveVT();
}

void InputController::OnLidEvent(LidState state) {
  LOG(INFO) << "Lid " << LidStateToString(state);

  lid_state_ = state;
  InputEvent proto;
  switch (lid_state_) {
    case LID_CLOSED:
      input_->SetTouchDevicesState(false);
      input_->SetWakeInputsState(false);
      delegate_->HandleLidClosed();
      proto.set_type(InputEvent_Type_LID_CLOSED);
      break;
    case LID_OPEN:
      input_->SetTouchDevicesState(true);
      input_->SetWakeInputsState(true);
      delegate_->HandleLidOpened();
      proto.set_type(InputEvent_Type_LID_OPEN);
      break;
    case LID_NOT_PRESENT:
      return;
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

  delegate_->HandlePowerButtonEvent(state);
}

gboolean InputController::CheckActiveVT() {
  if (input_->GetActiveVT() == 2)
    delegate_->DeferInactivityTimeoutForVT2();
  return TRUE;
}

}  // namespace policy
}  // namespace power_manager
