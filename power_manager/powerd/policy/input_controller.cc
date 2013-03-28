// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/powerd/policy/input_controller.h"

#include <glib.h>

#include "chromeos/dbus/service_constants.h"
#include "power_manager/common/dbus_sender.h"
#include "power_manager/common/prefs.h"
#include "power_manager/common/util.h"
#include "power_manager/input_event.pb.h"
#include "power_manager/powerd/backlight_controller.h"
#include "power_manager/powerd/policy/state_controller.h"
#include "power_manager/powerd/system/input.h"

namespace power_manager {
namespace policy {

namespace {

// Frequency with which CheckActiveVT() should be called, in seconds.
// This just needs to be lower than the screen-dimming delay.
const int kCheckActiveVTFrequencySec = 60;

} // namespace

InputController::InputController(system::Input* input,
                                 Delegate* delegate,
                                 BacklightController* backlight_controller,
                                 StateController* state_controller,
                                 DBusSenderInterface* dbus_sender,
                                 const base::FilePath& run_dir)
    : input_(input),
      delegate_(delegate),
      backlight_controller_(backlight_controller),
      state_controller_(state_controller),
      dbus_sender_(dbus_sender),
      lid_state_(LID_OPEN),
      use_input_for_lid_(true),
      check_active_vt_timeout_id_(0) {
  input_->AddObserver(this);
}

InputController::~InputController() {
  input_->RemoveObserver(this);
  util::RemoveTimeout(&check_active_vt_timeout_id_);
}

void InputController::Init(PrefsInterface* prefs) {
  CHECK(prefs->GetBool(kUseLidPref, &use_input_for_lid_));
  LidState lid_state = LID_OPEN;
  if (use_input_for_lid_ && input_->QueryLidState(&lid_state))
    OnLidEvent(lid_state);

  check_active_vt_timeout_id_ = g_timeout_add(
      kCheckActiveVTFrequencySec * 1000, CheckActiveVTThunk, this);
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
    backlight_controller_->HandlePowerButtonPress();
    LOG(INFO) << "Syncing state due to power button down event";
    util::Launch("sync");
  }
}

gboolean InputController::CheckActiveVT() {
  if (input_->GetActiveVT() == 2)
    state_controller_->HandleUserActivity();
  return TRUE;
}

}  // namespace policy
}  // namespace power_manager
