// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/powerd/policy/input_event_handler.h"

#include <base/logging.h>
#include <chromeos/dbus/service_constants.h>

#include "power_manager/common/clock.h"
#include "power_manager/common/power_constants.h"
#include "power_manager/common/prefs.h"
#include "power_manager/common/util.h"
#include "power_manager/powerd/system/dbus_wrapper.h"
#include "power_manager/powerd/system/display/display_watcher.h"
#include "power_manager/powerd/system/input_watcher_interface.h"
#include "power_manager/proto_bindings/input_event.pb.h"

namespace power_manager {
namespace policy {

InputEventHandler::InputEventHandler()
    : input_watcher_(NULL),
      delegate_(NULL),
      display_watcher_(NULL),
      dbus_wrapper_(NULL),
      clock_(new Clock),
      only_has_external_display_(false),
      factory_mode_(false),
      lid_state_(LidState::NOT_PRESENT),
      tablet_mode_(TabletMode::UNSUPPORTED),
      power_button_down_ignored_(false) {}

InputEventHandler::~InputEventHandler() {
  if (input_watcher_)
    input_watcher_->RemoveObserver(this);
}

void InputEventHandler::Init(system::InputWatcherInterface* input_watcher,
                             Delegate* delegate,
                             system::DisplayWatcherInterface* display_watcher,
                             system::DBusWrapperInterface* dbus_wrapper,
                             PrefsInterface* prefs) {
  input_watcher_ = input_watcher;
  input_watcher_->AddObserver(this);
  delegate_ = delegate;
  display_watcher_ = display_watcher;
  dbus_wrapper_ = dbus_wrapper;

  prefs->GetBool(kExternalDisplayOnlyPref, &only_has_external_display_);
  prefs->GetBool(kFactoryModePref, &factory_mode_);

  bool use_lid = false;
  if (prefs->GetBool(kUseLidPref, &use_lid) && use_lid)
    lid_state_ = input_watcher_->QueryLidState();

  tablet_mode_ = input_watcher_->GetTabletMode();
}

bool InputEventHandler::TriggerPowerButtonAcknowledgmentTimeoutForTesting() {
  if (!power_button_acknowledgment_timer_.IsRunning())
    return false;

  power_button_acknowledgment_timer_.Stop();
  HandlePowerButtonAcknowledgmentTimeout();
  return true;
}

void InputEventHandler::HandlePowerButtonAcknowledgment(
    const base::TimeTicks& timestamp) {
  VLOG(1) << "Received acknowledgment of power button press at "
          << timestamp.ToInternalValue() << "; expected "
          << expected_power_button_acknowledgment_timestamp_.ToInternalValue();
  if (timestamp == expected_power_button_acknowledgment_timestamp_) {
    delegate_->ReportPowerButtonAcknowledgmentDelay(
        clock_->GetCurrentTime() -
        expected_power_button_acknowledgment_timestamp_);
    expected_power_button_acknowledgment_timestamp_ = base::TimeTicks();
    power_button_acknowledgment_timer_.Stop();
  }
}

void InputEventHandler::IgnoreNextPowerButtonPress(
    const base::TimeDelta& timeout) {
  if (timeout.is_zero()) {
    VLOG(1) << "Cancel power button press discarding";
    ignore_power_button_deadline_ = base::TimeTicks();
    power_button_down_ignored_ = false;
  } else {
    VLOG(1) << "Ignoring power button for " << timeout.InMilliseconds()
            << " ms";
    ignore_power_button_deadline_ = clock_->GetCurrentTime() + timeout;
  }
}

void InputEventHandler::OnLidEvent(LidState state) {
  lid_state_ = state;
  InputEvent proto;
  switch (lid_state_) {
    case LidState::CLOSED:
      delegate_->HandleLidClosed();
      proto.set_type(InputEvent_Type_LID_CLOSED);
      break;
    case LidState::OPEN:
      delegate_->HandleLidOpened();
      proto.set_type(InputEvent_Type_LID_OPEN);
      break;
    case LidState::NOT_PRESENT:
      return;
  }
  proto.set_timestamp(clock_->GetCurrentTime().ToInternalValue());
  dbus_wrapper_->EmitSignalWithProtocolBuffer(kInputEventSignal, proto);
}

void InputEventHandler::OnTabletModeEvent(TabletMode mode) {
  DCHECK_NE(mode, TabletMode::UNSUPPORTED);
  tablet_mode_ = mode;

  delegate_->HandleTabletModeChange(mode);

  InputEvent proto;
  proto.set_type(tablet_mode_ == TabletMode::ON
                     ? InputEvent_Type_TABLET_MODE_ON
                     : InputEvent_Type_TABLET_MODE_OFF);
  proto.set_timestamp(clock_->GetCurrentTime().ToInternalValue());
  dbus_wrapper_->EmitSignalWithProtocolBuffer(kInputEventSignal, proto);
}

void InputEventHandler::OnPowerButtonEvent(ButtonState state) {
  if (factory_mode_) {
    LOG(INFO) << "Ignoring power button " << ButtonStateToString(state)
              << " for factory mode";
    return;
  }

  if (clock_->GetCurrentTime() < ignore_power_button_deadline_) {
    bool ignore = state == ButtonState::DOWN || power_button_down_ignored_;
    if (state == ButtonState::UP)  // Consumed, we no longer need the deadline.
      IgnoreNextPowerButtonPress(base::TimeDelta());
    else if (state == ButtonState::DOWN)
      power_button_down_ignored_ = true;
    if (ignore) {
      // Ignore down event or up event if it matches a down event.
      LOG(INFO) << "Ignored power button " << ButtonStateToString(state);
      // Do not forward this event.
      return;
    }
  }

  if (state == ButtonState::DOWN && only_has_external_display_ &&
      display_watcher_->GetDisplays().empty()) {
    delegate_->ShutDownForPowerButtonWithNoDisplay();
    return;
  }

  if (state != ButtonState::REPEAT) {
    const base::TimeTicks now = clock_->GetCurrentTime();

    InputEvent proto;
    proto.set_type(state == ButtonState::DOWN
                       ? InputEvent_Type_POWER_BUTTON_DOWN
                       : InputEvent_Type_POWER_BUTTON_UP);
    proto.set_timestamp(now.ToInternalValue());
    dbus_wrapper_->EmitSignalWithProtocolBuffer(kInputEventSignal, proto);

    if (state == ButtonState::DOWN) {
      expected_power_button_acknowledgment_timestamp_ = now;
      power_button_acknowledgment_timer_.Start(
          FROM_HERE,
          base::TimeDelta::FromMilliseconds(
              kPowerButtonAcknowledgmentTimeoutMs),
          this, &InputEventHandler::HandlePowerButtonAcknowledgmentTimeout);
    } else {
      expected_power_button_acknowledgment_timestamp_ = base::TimeTicks();
      power_button_acknowledgment_timer_.Stop();
    }
  }

  delegate_->HandlePowerButtonEvent(state);
}

void InputEventHandler::OnHoverStateChange(bool hovering) {
  delegate_->HandleHoverStateChange(hovering);
}

void InputEventHandler::HandlePowerButtonAcknowledgmentTimeout() {
  delegate_->ReportPowerButtonAcknowledgmentDelay(
      base::TimeDelta::FromMilliseconds(kPowerButtonAcknowledgmentTimeoutMs));
  delegate_->HandleMissingPowerButtonAcknowledgment();
  expected_power_button_acknowledgment_timestamp_ = base::TimeTicks();
}

}  // namespace policy
}  // namespace power_manager
