// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/powerd/policy/backlight_controller_test_util.h"

#include <cmath>
#include <memory>

#include <chromeos/dbus/service_constants.h>
#include <dbus/message.h>
#include <gtest/gtest.h>

#include "power_manager/powerd/system/dbus_wrapper_stub.h"

namespace power_manager {
namespace policy {
namespace test {

void CallIncreaseScreenBrightness(system::DBusWrapperStub* wrapper) {
  DCHECK(wrapper);
  dbus::MethodCall method_call(kPowerManagerInterface,
                               kIncreaseScreenBrightnessMethod);
  ASSERT_TRUE(wrapper->CallExportedMethodSync(&method_call));
}

void CallDecreaseScreenBrightness(system::DBusWrapperStub* wrapper,
                                  bool allow_off) {
  DCHECK(wrapper);
  dbus::MethodCall method_call(kPowerManagerInterface,
                               kDecreaseScreenBrightnessMethod);
  dbus::MessageWriter(&method_call).AppendBool(allow_off);
  ASSERT_TRUE(wrapper->CallExportedMethodSync(&method_call));
}

void CallSetScreenBrightnessPercent(
    system::DBusWrapperStub* wrapper,
    double percent,
    SetBacklightBrightnessRequest_Transition transition,
    SetBacklightBrightnessRequest_Cause cause) {
  DCHECK(wrapper);
  dbus::MethodCall method_call(kPowerManagerInterface,
                               kSetScreenBrightnessPercentMethod);
  dbus::MessageWriter writer(&method_call);
  SetBacklightBrightnessRequest proto;
  proto.set_percent(percent);
  proto.set_transition(transition);
  proto.set_cause(cause);
  writer.AppendProtoAsArrayOfBytes(proto);
  ASSERT_TRUE(wrapper->CallExportedMethodSync(&method_call));
}

void CallSetScreenBrightnessPercentLegacy(system::DBusWrapperStub* wrapper,
                                          double percent,
                                          int transition) {
  DCHECK(wrapper);
  dbus::MethodCall method_call(kPowerManagerInterface,
                               kSetScreenBrightnessPercentMethod);
  dbus::MessageWriter writer(&method_call);
  writer.AppendDouble(percent);
  writer.AppendInt32(transition);
  ASSERT_TRUE(wrapper->CallExportedMethodSync(&method_call));
}

void CheckBrightnessChangedSignal(system::DBusWrapperStub* wrapper,
                                  size_t index,
                                  const std::string& signal_name,
                                  double brightness_percent,
                                  BacklightBrightnessChange_Cause cause) {
  std::unique_ptr<dbus::Signal> signal;
  ASSERT_TRUE(wrapper->GetSentSignal(index, signal_name, nullptr, &signal));

  BacklightBrightnessChange proto;
  ASSERT_TRUE(dbus::MessageReader(signal.get()).PopArrayOfBytesAsProto(&proto));
  EXPECT_DOUBLE_EQ(round(brightness_percent), round(proto.percent()));
  EXPECT_EQ(cause, proto.cause());
}

}  // namespace test
}  // namespace policy
}  // namespace power_manager
