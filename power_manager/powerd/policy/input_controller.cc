// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/powerd/policy/input_controller.h"

#include "chromeos/dbus/service_constants.h"
#include "power_manager/common/dbus_sender.h"
#include "power_manager/common/power_prefs.h"
#include "power_manager/common/util.h"
#include "power_manager/input_event.pb.h"
#include "power_manager/powerd/system/input.h"

namespace power_manager {
namespace policy {

InputController::InputController(system::Input* input,
                                 Delegate* delegate,
                                 DBusSenderInterface* dbus_sender,
                                 const FilePath& run_dir)
    : input_(input),
      delegate_(delegate),
      dbus_sender_(dbus_sender),
      lid_state_(LID_STATE_OPENED),
      use_input_for_lid_(true),
      lid_open_file_(run_dir.Append(kLidOpenFile)) {
  input_->AddObserver(this);

}

InputController::~InputController() {
  input_->RemoveObserver(this);
}

void InputController::Init(PowerPrefs* prefs) {
  CHECK(prefs->GetBool(kUseLidPref, &use_input_for_lid_));
  int lid_state_value = 0;
  if (use_input_for_lid_ && input_->QueryLidState(&lid_state_value))
    OnInputEvent(INPUT_LID, lid_state_value);
}

void InputController::OnInputEvent(InputType type, int value) {
  switch (type) {
    case INPUT_LID: {
      lid_state_ = GetLidState(value);
      LOG(INFO) << "Lid "
                << (lid_state_ == LID_STATE_CLOSED ? "closed." : "opened.");
      if (!use_input_for_lid_) {
        LOG(INFO) << "Ignoring lid.";
        break;
      }
      if (lid_state_ == LID_STATE_CLOSED) {
        input_->SetTouchDevicesState(false);
        input_->SetWakeInputsState(false);
        util::RemoveStatusFile(lid_open_file_);
        SendInputEventSignal(INPUT_LID, BUTTON_DOWN);
        delegate_->StartSuspendForLidClose();
      } else {
        input_->SetTouchDevicesState(true);
        input_->SetWakeInputsState(true);
        util::CreateStatusFile(lid_open_file_);
        SendInputEventSignal(INPUT_LID, BUTTON_UP);
        delegate_->CancelSuspendForLidOpen();
      }
      break;
    }
    case INPUT_POWER_BUTTON: {
      ButtonState button_state = GetButtonState(value);
      SendInputEventSignal(type, button_state);
      delegate_->SendPowerButtonMetric(button_state == BUTTON_DOWN,
                                       base::TimeTicks::Now());
      if (button_state == BUTTON_DOWN) {
        delegate_->EnsureBacklightIsOn();
        LOG(INFO) << "Syncing state due to power button down event";
        util::Launch("sync");
      }
      break;
    }
    default: {
      NOTREACHED() << "Bad input type " << type;
      break;
    }
  }
}

// static
InputController::LidState InputController::GetLidState(int value) {
  return value == 0 ? LID_STATE_OPENED : LID_STATE_CLOSED;
}

// static
InputController::ButtonState InputController::GetButtonState(int value) {
  // value == 0 is button up.
  // value == 1 is button down.
  // value == 2 is key repeat.
  return static_cast<ButtonState>(value);
}

void InputController::SendInputEventSignal(InputType type, ButtonState state) {
  if (state == BUTTON_REPEAT)
    return;
  LOG(INFO) << "Sending input event signal: " << util::InputTypeToString(type)
            << " is " << (state == BUTTON_UP ? "up" : "down");

  InputEvent proto;
  switch (type) {
    case INPUT_LID:
      proto.set_type(state == BUTTON_DOWN ?
                     InputEvent_Type_LID_CLOSED :
                     InputEvent_Type_LID_OPEN);
      break;
    case INPUT_POWER_BUTTON:
      proto.set_type(state == BUTTON_DOWN ?
                     InputEvent_Type_POWER_BUTTON_DOWN :
                     InputEvent_Type_POWER_BUTTON_UP);
      break;
    default:
      NOTREACHED() << "Unhandled input event type " << type;
  }
  proto.set_timestamp(base::TimeTicks::Now().ToInternalValue());
  dbus_sender_->EmitSignalWithProtocolBuffer(kInputEventSignal, proto);
}

}  // namespace policy
}  // namespace power_manager
