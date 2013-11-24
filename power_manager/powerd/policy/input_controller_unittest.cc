// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/powerd/policy/input_controller.h"

#include <gtest/gtest.h>

#include "chromeos/dbus/service_constants.h"
#include "power_manager/common/clock.h"
#include "power_manager/common/dbus_sender_stub.h"
#include "power_manager/common/fake_prefs.h"
#include "power_manager/common/power_constants.h"
#include "power_manager/common/test_util.h"
#include "power_manager/input_event.pb.h"
#include "power_manager/powerd/system/input_stub.h"

namespace power_manager {
namespace policy {
namespace {

using test::AppendAction;

const char kNoActions[] = "";
const char kLidClosed[] = "lid_closed";
const char kLidOpened[] = "lid_opened";
const char kPowerButtonDown[] = "power_down";
const char kPowerButtonUp[] = "power_up";
const char kDeferInactivity[] = "defer_inactivity";
const char kShutDown[] = "shut_down";
const char kMissingPowerButtonAcknowledgment[] = "missing_power_button_ack";

class TestInputControllerDelegate : public InputController::Delegate {
 public:
  TestInputControllerDelegate() {}
  virtual ~TestInputControllerDelegate() {}

  // Returns a comma-separated string describing the actions that were
  // requested since the previous call to GetActions() (i.e. results are
  // non-repeatable).
  std::string GetActions() {
    std::string actions = actions_;
    actions_.clear();
    return actions;
  }

  // InputController::Delegate implementation:
  virtual void HandleLidClosed() OVERRIDE {
    AppendAction(&actions_, kLidClosed);
  }
  virtual void HandleLidOpened() OVERRIDE {
    AppendAction(&actions_, kLidOpened);
  }
  virtual void HandlePowerButtonEvent(ButtonState state) {
    AppendAction(&actions_,
                 state == BUTTON_DOWN ? kPowerButtonDown : kPowerButtonUp);
  }
  virtual void DeferInactivityTimeoutForVT2() OVERRIDE {
    AppendAction(&actions_, kDeferInactivity);
  }
  virtual void ShutDownForPowerButtonWithNoDisplay() OVERRIDE {
    AppendAction(&actions_, kShutDown);
  }
  virtual void HandleMissingPowerButtonAcknowledgment() OVERRIDE {
    AppendAction(&actions_, kMissingPowerButtonAcknowledgment);
  }

 private:
  std::string actions_;

  DISALLOW_COPY_AND_ASSIGN(TestInputControllerDelegate);
};

}  // namespace

class InputControllerTest : public ::testing::Test {
 public:
  InputControllerTest() : controller_(&input_, &delegate_, &dbus_sender_) {
    controller_.clock_for_testing()->set_current_time_for_testing(
        base::TimeTicks::FromInternalValue(1000));
  }
  virtual ~InputControllerTest() {}

 protected:
  // Tests that one InputEvent D-Bus signal has been sent and returns the
  // signal's |type| field.
  int GetInputEventSignalType() {
    InputEvent proto;
    EXPECT_EQ(1, dbus_sender_.num_sent_signals());
    EXPECT_TRUE(dbus_sender_.GetSentSignal(0, kInputEventSignal, &proto));
    return proto.type();
  }

  // Tests that one InputEvent D-Bus signal has been sent and returns the
  // signal's |timestamp| field.
  int64 GetInputEventSignalTimestamp() {
    InputEvent proto;
    EXPECT_EQ(1, dbus_sender_.num_sent_signals());
    EXPECT_TRUE(dbus_sender_.GetSentSignal(0, kInputEventSignal, &proto));
    return proto.timestamp();
  }

  // Returns the current (fake) time.
  base::TimeTicks Now() {
    return controller_.clock_for_testing()->GetCurrentTime();
  }

  // Advances the current time by |interval|.
  void AdvanceTime(const base::TimeDelta& interval) {
    controller_.clock_for_testing()->set_current_time_for_testing(
        Now() + interval);
  }

  FakePrefs prefs_;
  system::InputStub input_;
  DBusSenderStub dbus_sender_;
  TestInputControllerDelegate delegate_;
  InputController controller_;
};

TEST_F(InputControllerTest, LidEvents) {
  EXPECT_EQ(kNoActions, delegate_.GetActions());

  // An initial event about the lid state should be sent at initialization.
  prefs_.SetInt64(kUseLidPref, 1);
  controller_.Init(&prefs_);
  EXPECT_TRUE(input_.wake_inputs_enabled());
  EXPECT_TRUE(input_.touch_devices_enabled());
  EXPECT_EQ(kLidOpened, delegate_.GetActions());
  EXPECT_EQ(InputEvent_Type_LID_OPEN, GetInputEventSignalType());
  EXPECT_EQ(Now().ToInternalValue(), GetInputEventSignalTimestamp());
  dbus_sender_.ClearSentSignals();

  AdvanceTime(base::TimeDelta::FromSeconds(1));
  input_.set_lid_state(LID_CLOSED);
  input_.NotifyObserversAboutLidState();
  EXPECT_FALSE(input_.wake_inputs_enabled());
  EXPECT_FALSE(input_.touch_devices_enabled());
  EXPECT_EQ(kLidClosed, delegate_.GetActions());
  EXPECT_EQ(InputEvent_Type_LID_CLOSED, GetInputEventSignalType());
  EXPECT_EQ(Now().ToInternalValue(), GetInputEventSignalTimestamp());
  dbus_sender_.ClearSentSignals();

  AdvanceTime(base::TimeDelta::FromSeconds(5));
  input_.set_lid_state(LID_OPEN);
  input_.NotifyObserversAboutLidState();
  EXPECT_TRUE(input_.wake_inputs_enabled());
  EXPECT_TRUE(input_.touch_devices_enabled());
  EXPECT_EQ(kLidOpened, delegate_.GetActions());
  EXPECT_EQ(InputEvent_Type_LID_OPEN, GetInputEventSignalType());
  EXPECT_EQ(Now().ToInternalValue(), GetInputEventSignalTimestamp());
  dbus_sender_.ClearSentSignals();
}

TEST_F(InputControllerTest, PowerButtonEvents) {
  prefs_.SetInt64(kExternalDisplayOnlyPref, 1);
  input_.set_display_connected(true);
  controller_.Init(&prefs_);

  input_.NotifyObserversAboutPowerButtonEvent(BUTTON_DOWN);
  EXPECT_EQ(kPowerButtonDown, delegate_.GetActions());
  EXPECT_EQ(InputEvent_Type_POWER_BUTTON_DOWN, GetInputEventSignalType());
  EXPECT_EQ(Now().ToInternalValue(), GetInputEventSignalTimestamp());
  dbus_sender_.ClearSentSignals();

  AdvanceTime(base::TimeDelta::FromMilliseconds(100));
  input_.NotifyObserversAboutPowerButtonEvent(BUTTON_UP);
  EXPECT_EQ(kPowerButtonUp, delegate_.GetActions());
  EXPECT_EQ(InputEvent_Type_POWER_BUTTON_UP, GetInputEventSignalType());
  EXPECT_EQ(Now().ToInternalValue(), GetInputEventSignalTimestamp());
  dbus_sender_.ClearSentSignals();

  // With no displays connected, the system should shut down immediately.
  input_.set_display_connected(false);
  input_.NotifyObserversAboutPowerButtonEvent(BUTTON_DOWN);
  EXPECT_EQ(kShutDown, delegate_.GetActions());
  EXPECT_EQ(0, dbus_sender_.num_sent_signals());
}

TEST_F(InputControllerTest, DeferInactivityTimeoutWhileVT2IsActive) {
  controller_.Init(&prefs_);

  input_.set_active_vt(1);
  EXPECT_TRUE(controller_.TriggerCheckActiveVTTimeoutForTesting());
  EXPECT_EQ(kNoActions, delegate_.GetActions());

  input_.set_active_vt(2);
  EXPECT_TRUE(controller_.TriggerCheckActiveVTTimeoutForTesting());
  EXPECT_EQ(kDeferInactivity, delegate_.GetActions());

  input_.set_active_vt(3);
  EXPECT_TRUE(controller_.TriggerCheckActiveVTTimeoutForTesting());
  EXPECT_EQ(kNoActions, delegate_.GetActions());
}

TEST_F(InputControllerTest, AcknowledgePowerButtonPresses) {
  controller_.Init(&prefs_);

  // Press the power button, acknowledge the event immediately, and check that
  // no further actions are performed and that the timeout is stopped.
  input_.NotifyObserversAboutPowerButtonEvent(BUTTON_DOWN);
  EXPECT_EQ(kPowerButtonDown, delegate_.GetActions());
  controller_.HandlePowerButtonAcknowledgment(
      base::TimeTicks::FromInternalValue(GetInputEventSignalTimestamp()));
  EXPECT_EQ(kNoActions, delegate_.GetActions());
  ASSERT_FALSE(controller_.TriggerPowerButtonAcknowledgmentTimeoutForTesting());
  input_.NotifyObserversAboutPowerButtonEvent(BUTTON_UP);
  EXPECT_EQ(kPowerButtonUp, delegate_.GetActions());

  // Check that releasing the power button before it's been acknowledged also
  // stops the timeout.
  AdvanceTime(base::TimeDelta::FromSeconds(1));
  input_.NotifyObserversAboutPowerButtonEvent(BUTTON_DOWN);
  EXPECT_EQ(kPowerButtonDown, delegate_.GetActions());
  input_.NotifyObserversAboutPowerButtonEvent(BUTTON_UP);
  EXPECT_EQ(kPowerButtonUp, delegate_.GetActions());
  ASSERT_FALSE(controller_.TriggerPowerButtonAcknowledgmentTimeoutForTesting());
  dbus_sender_.ClearSentSignals();

  // Let the timeout fire and check that the delegate is notified.
  AdvanceTime(base::TimeDelta::FromSeconds(1));
  input_.NotifyObserversAboutPowerButtonEvent(BUTTON_DOWN);
  EXPECT_EQ(kPowerButtonDown, delegate_.GetActions());
  ASSERT_TRUE(controller_.TriggerPowerButtonAcknowledgmentTimeoutForTesting());
  EXPECT_EQ(kMissingPowerButtonAcknowledgment, delegate_.GetActions());
  ASSERT_FALSE(controller_.TriggerPowerButtonAcknowledgmentTimeoutForTesting());
  input_.NotifyObserversAboutPowerButtonEvent(BUTTON_UP);
  EXPECT_EQ(kPowerButtonUp, delegate_.GetActions());

  // Send an acknowledgment with a stale timestamp and check that it doesn't
  // stop the timeout.
  AdvanceTime(base::TimeDelta::FromSeconds(1));
  dbus_sender_.ClearSentSignals();
  input_.NotifyObserversAboutPowerButtonEvent(BUTTON_DOWN);
  EXPECT_EQ(kPowerButtonDown, delegate_.GetActions());
  controller_.HandlePowerButtonAcknowledgment(
      base::TimeTicks::FromInternalValue(GetInputEventSignalTimestamp() - 100));
  EXPECT_EQ(kNoActions, delegate_.GetActions());
  ASSERT_TRUE(controller_.TriggerPowerButtonAcknowledgmentTimeoutForTesting());
  EXPECT_EQ(kMissingPowerButtonAcknowledgment, delegate_.GetActions());
  ASSERT_FALSE(controller_.TriggerPowerButtonAcknowledgmentTimeoutForTesting());
  input_.NotifyObserversAboutPowerButtonEvent(BUTTON_UP);
  EXPECT_EQ(kPowerButtonUp, delegate_.GetActions());
}

}  // namespace policy
}  // namespace power_manager
