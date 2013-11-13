// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/powerd/policy/input_controller.h"

#include <gtest/gtest.h>

#include "chromeos/dbus/service_constants.h"
#include "power_manager/common/dbus_sender_stub.h"
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

 private:
  std::string actions_;

  DISALLOW_COPY_AND_ASSIGN(TestInputControllerDelegate);
};

}  // namespace

class InputControllerTest : public ::testing::Test {
 public:
  InputControllerTest() : controller_(&input_, &delegate_, &dbus_sender_) {}
  virtual ~InputControllerTest() {}

 protected:
  // Tests that one InputEvent D-Bus signal has been sent, clears sent signals,
  // and returns the signal's |type| field.
  int GetInputEventSignalType() {
    InputEvent proto;
    EXPECT_EQ(1, dbus_sender_.num_sent_signals());
    EXPECT_TRUE(dbus_sender_.GetSentSignal(0, kInputEventSignal, &proto));
    dbus_sender_.ClearSentSignals();
    return proto.type();
  }

  system::InputStub input_;
  DBusSenderStub dbus_sender_;
  TestInputControllerDelegate delegate_;
  InputController controller_;
};

TEST_F(InputControllerTest, LidEvents) {
  EXPECT_EQ(kNoActions, delegate_.GetActions());

  // An initial event about the lid state should be sent at initialization.
  controller_.Init();
  EXPECT_TRUE(input_.wake_inputs_enabled());
  EXPECT_TRUE(input_.touch_devices_enabled());
  EXPECT_EQ(kLidOpened, delegate_.GetActions());
  EXPECT_EQ(InputEvent_Type_LID_OPEN, GetInputEventSignalType());

  input_.set_lid_state(LID_CLOSED);
  input_.NotifyObserversAboutLidState();
  EXPECT_FALSE(input_.wake_inputs_enabled());
  EXPECT_FALSE(input_.touch_devices_enabled());
  EXPECT_EQ(kLidClosed, delegate_.GetActions());
  EXPECT_EQ(InputEvent_Type_LID_CLOSED, GetInputEventSignalType());

  input_.set_lid_state(LID_OPEN);
  input_.NotifyObserversAboutLidState();
  EXPECT_TRUE(input_.wake_inputs_enabled());
  EXPECT_TRUE(input_.touch_devices_enabled());
  EXPECT_EQ(kLidOpened, delegate_.GetActions());
  EXPECT_EQ(InputEvent_Type_LID_OPEN, GetInputEventSignalType());
}

TEST_F(InputControllerTest, PowerButtonEvents) {
  controller_.Init();
  delegate_.GetActions();
  dbus_sender_.ClearSentSignals();

  input_.NotifyObserversAboutPowerButtonEvent(BUTTON_DOWN);
  EXPECT_EQ(kPowerButtonDown, delegate_.GetActions());
  EXPECT_EQ(InputEvent_Type_POWER_BUTTON_DOWN, GetInputEventSignalType());

  input_.NotifyObserversAboutPowerButtonEvent(BUTTON_UP);
  EXPECT_EQ(kPowerButtonUp, delegate_.GetActions());
  EXPECT_EQ(InputEvent_Type_POWER_BUTTON_UP, GetInputEventSignalType());
}

TEST_F(InputControllerTest, DeferInactivityTimeoutWhileVT2IsActive) {
  controller_.Init();
  delegate_.GetActions();

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

}  // namespace policy
}  // namespace power_manager
