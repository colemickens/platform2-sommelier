// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/powerd/policy/input_controller.h"

#include <string>
#include <vector>

#include <base/format_macros.h>
#include <base/strings/stringprintf.h>
#include <chromeos/dbus/service_constants.h>
#include <gtest/gtest.h>

#include "power_manager/common/action_recorder.h"
#include "power_manager/common/clock.h"
#include "power_manager/common/dbus_sender_stub.h"
#include "power_manager/common/fake_prefs.h"
#include "power_manager/common/power_constants.h"
#include "power_manager/powerd/system/display/display_info.h"
#include "power_manager/powerd/system/display/display_watcher_stub.h"
#include "power_manager/powerd/system/input_stub.h"
#include "power_manager/proto_bindings/input_event.pb.h"

namespace power_manager {
namespace policy {
namespace {

const char kNoActions[] = "";
const char kLidClosed[] = "lid_closed";
const char kLidOpened[] = "lid_opened";
const char kPowerButtonDown[] = "power_down";
const char kPowerButtonUp[] = "power_up";
const char kDeferInactivity[] = "defer_inactivity";
const char kShutDown[] = "shut_down";
const char kMissingPowerButtonAcknowledgment[] = "missing_power_button_ack";

std::string GetAcknowledgmentDelayAction(base::TimeDelta delay) {
  return base::StringPrintf("power_button_ack_delay(%" PRId64 ")",
                            delay.InMilliseconds());
}

class TestInputControllerDelegate : public InputController::Delegate,
                                    public ActionRecorder {
 public:
  TestInputControllerDelegate() {}
  virtual ~TestInputControllerDelegate() {}

  // InputController::Delegate implementation:
  virtual void HandleLidClosed() OVERRIDE {
    AppendAction(kLidClosed);
  }
  virtual void HandleLidOpened() OVERRIDE {
    AppendAction(kLidOpened);
  }
  virtual void HandlePowerButtonEvent(ButtonState state) {
    AppendAction(state == BUTTON_DOWN ? kPowerButtonDown : kPowerButtonUp);
  }
  virtual void DeferInactivityTimeoutForVT2() OVERRIDE {
    AppendAction(kDeferInactivity);
  }
  virtual void ShutDownForPowerButtonWithNoDisplay() OVERRIDE {
    AppendAction(kShutDown);
  }
  virtual void HandleMissingPowerButtonAcknowledgment() OVERRIDE {
    AppendAction(kMissingPowerButtonAcknowledgment);
  }
  virtual void ReportPowerButtonAcknowledgmentDelay(base::TimeDelta delay)
      OVERRIDE {
    AppendAction(GetAcknowledgmentDelayAction(delay));
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestInputControllerDelegate);
};

}  // namespace

class InputControllerTest : public ::testing::Test {
 public:
  InputControllerTest() {
    controller_.clock_for_testing()->set_current_time_for_testing(
        base::TimeTicks::FromInternalValue(1000));
  }
  virtual ~InputControllerTest() {}

 protected:
  // Initializes |controller_|.
  void Init() {
    controller_.Init(
        &input_, &delegate_, &display_watcher_, &dbus_sender_, &prefs_);
  }

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
  system::DisplayWatcherStub display_watcher_;
  DBusSenderStub dbus_sender_;
  TestInputControllerDelegate delegate_;
  InputController controller_;
};

TEST_F(InputControllerTest, LidEvents) {
  EXPECT_EQ(kNoActions, delegate_.GetActions());

  // An initial event about the lid state should be sent at initialization.
  prefs_.SetInt64(kUseLidPref, 1);
  Init();
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
  std::vector<system::DisplayInfo> displays(1, system::DisplayInfo());
  display_watcher_.set_displays(displays);
  Init();

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
  displays.clear();
  display_watcher_.set_displays(displays);
  input_.NotifyObserversAboutPowerButtonEvent(BUTTON_DOWN);
  EXPECT_EQ(kShutDown, delegate_.GetActions());
  EXPECT_EQ(0, dbus_sender_.num_sent_signals());
}

TEST_F(InputControllerTest, DeferInactivityTimeoutWhileVT2IsActive) {
  Init();

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
  Init();

  const base::TimeDelta kShortDelay = base::TimeDelta::FromMilliseconds(100);
  const base::TimeDelta kTimeout = base::TimeDelta::FromMilliseconds(
      InputController::kPowerButtonAcknowledgmentTimeoutMs);

  // Press the power button, acknowledge the event nearly immediately, and check
  // that no further actions are performed and that the timeout is stopped.
  input_.NotifyObserversAboutPowerButtonEvent(BUTTON_DOWN);
  EXPECT_EQ(kPowerButtonDown, delegate_.GetActions());
  AdvanceTime(kShortDelay);
  controller_.HandlePowerButtonAcknowledgment(
      base::TimeTicks::FromInternalValue(GetInputEventSignalTimestamp()));
  EXPECT_EQ(GetAcknowledgmentDelayAction(kShortDelay), delegate_.GetActions());
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
  EXPECT_EQ(JoinActions(GetAcknowledgmentDelayAction(kTimeout).c_str(),
                        kMissingPowerButtonAcknowledgment, NULL),
            delegate_.GetActions());
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
  EXPECT_EQ(JoinActions(GetAcknowledgmentDelayAction(kTimeout).c_str(),
                        kMissingPowerButtonAcknowledgment, NULL),
            delegate_.GetActions());
  ASSERT_FALSE(controller_.TriggerPowerButtonAcknowledgmentTimeoutForTesting());
  input_.NotifyObserversAboutPowerButtonEvent(BUTTON_UP);
  EXPECT_EQ(kPowerButtonUp, delegate_.GetActions());
}

}  // namespace policy
}  // namespace power_manager
