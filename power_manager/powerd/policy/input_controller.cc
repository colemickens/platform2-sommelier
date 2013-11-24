// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/powerd/policy/input_controller.h"

#include <glib.h>

#include "base/logging.h"
#include "chromeos/dbus/service_constants.h"
#include "power_manager/common/clock.h"
#include "power_manager/common/dbus_sender.h"
#include "power_manager/common/power_constants.h"
#include "power_manager/common/prefs.h"
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
      clock_(new Clock),
      only_has_external_display_(false),
      lid_state_(LID_NOT_PRESENT),
      power_button_acknowledgment_timeout_id_(0),
      check_active_vt_timeout_id_(0) {
  input_->AddObserver(this);
}

InputController::~InputController() {
  input_->RemoveObserver(this);
  util::RemoveTimeout(&power_button_acknowledgment_timeout_id_);
  util::RemoveTimeout(&check_active_vt_timeout_id_);
}

void InputController::Init(PrefsInterface* prefs) {
  prefs->GetBool(kExternalDisplayOnlyPref, &only_has_external_display_);

  bool use_lid = false;
  if (prefs->GetBool(kUseLidPref, &use_lid) && use_lid)
    OnLidEvent(input_->QueryLidState());

  check_active_vt_timeout_id_ = g_timeout_add(
      kCheckActiveVTFrequencySec * 1000, CheckActiveVTThunk, this);
}

bool InputController::TriggerPowerButtonAcknowledgmentTimeoutForTesting() {
  if (!power_button_acknowledgment_timeout_id_)
    return false;

  guint old_id = power_button_acknowledgment_timeout_id_;
  if (!HandlePowerButtonAcknowledgmentTimeout())
    util::RemoveTimeout(&old_id);
  return true;
}

bool InputController::TriggerCheckActiveVTTimeoutForTesting() {
  if (!check_active_vt_timeout_id_)
    return false;

  return CheckActiveVT();
}

void InputController::HandlePowerButtonAcknowledgment(
    const base::TimeTicks& timestamp) {
  VLOG(1) << "Received acknowledgment of power button press at "
          << timestamp.ToInternalValue() << "; expected "
          << expected_power_button_acknowledgment_timestamp_.ToInternalValue();
  if (timestamp == expected_power_button_acknowledgment_timestamp_) {
    util::RemoveTimeout(&power_button_acknowledgment_timeout_id_);
    expected_power_button_acknowledgment_timestamp_ = base::TimeTicks();
  }
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
  proto.set_timestamp(clock_->GetCurrentTime().ToInternalValue());
  dbus_sender_->EmitSignalWithProtocolBuffer(kInputEventSignal, proto);
}

void InputController::OnPowerButtonEvent(ButtonState state) {
  LOG(INFO) << "Power button " << ButtonStateToString(state);

  if (state == BUTTON_DOWN && only_has_external_display_ &&
      !input_->IsDisplayConnected()) {
    delegate_->ShutDownForPowerButtonWithNoDisplay();
    return;
  }

  const base::TimeTicks now = clock_->GetCurrentTime();

  if (state != BUTTON_REPEAT) {
    InputEvent proto;
    proto.set_type(state == BUTTON_DOWN ?
                   InputEvent_Type_POWER_BUTTON_DOWN :
                   InputEvent_Type_POWER_BUTTON_UP);
    proto.set_timestamp(now.ToInternalValue());
    dbus_sender_->EmitSignalWithProtocolBuffer(kInputEventSignal, proto);

    if (state == BUTTON_DOWN) {
      expected_power_button_acknowledgment_timestamp_ = now;
      util::RemoveTimeout(&power_button_acknowledgment_timeout_id_);
      power_button_acknowledgment_timeout_id_ = g_timeout_add(
          kPowerButtonAcknowledgmentTimeoutMs,
          HandlePowerButtonAcknowledgmentTimeoutThunk, this);
    } else {
      expected_power_button_acknowledgment_timestamp_ = base::TimeTicks();
      util::RemoveTimeout(&power_button_acknowledgment_timeout_id_);
    }
  }

  delegate_->HandlePowerButtonEvent(state);
}

gboolean InputController::CheckActiveVT() {
  if (input_->GetActiveVT() == 2)
    delegate_->DeferInactivityTimeoutForVT2();
  return TRUE;
}

gboolean InputController::HandlePowerButtonAcknowledgmentTimeout() {
  delegate_->HandleMissingPowerButtonAcknowledgment();
  expected_power_button_acknowledgment_timestamp_ = base::TimeTicks();
  power_button_acknowledgment_timeout_id_ = 0;
  return FALSE;
}

}  // namespace policy
}  // namespace power_manager
