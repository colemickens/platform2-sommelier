// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/powerd/system/display/display_power_setter.h"

#include <memory>
#include <string>

#include <base/bind.h>
#include <base/logging.h>
#include <dbus/message.h>

#include "power_manager/common/util.h"
#include "power_manager/powerd/system/dbus_wrapper.h"

namespace power_manager {
namespace system {

namespace {

// Timeout for D-Bus method calls to Chrome.
const int kDisplayServiceDBusTimeoutMs = 5000;

std::string DisplayPowerStateToString(chromeos::DisplayPowerState state) {
  switch (state) {
    case chromeos::DISPLAY_POWER_ALL_ON:
      return "all displays on";
    case chromeos::DISPLAY_POWER_ALL_OFF:
      return "all displays off";
    case chromeos::DISPLAY_POWER_INTERNAL_OFF_EXTERNAL_ON:
      return "internal display off and external displays on";
    case chromeos::DISPLAY_POWER_INTERNAL_ON_EXTERNAL_OFF:
      return "internal display on and external displays off";
    default:
      return "[unknown]";
  }
}

}  // namespace

DisplayPowerSetter::DisplayPowerSetter()
    : dbus_wrapper_(nullptr), display_service_proxy_(nullptr) {}

DisplayPowerSetter::~DisplayPowerSetter() {}

void DisplayPowerSetter::Init(DBusWrapperInterface* dbus_wrapper) {
  DCHECK(dbus_wrapper);
  dbus_wrapper_ = dbus_wrapper;
  display_service_proxy_ = dbus_wrapper_->GetObjectProxy(
      chromeos::kDisplayServiceName, chromeos::kDisplayServicePath);
}

void DisplayPowerSetter::SetDisplayPower(chromeos::DisplayPowerState state,
                                         base::TimeDelta delay) {
  if (delay.InMilliseconds() == 0) {
    timer_.Stop();
    SendStateToDisplayService(state);
  } else {
    timer_.Start(FROM_HERE, delay,
                 base::Bind(&DisplayPowerSetter::SendStateToDisplayService,
                            base::Unretained(this), state));
  }
}

void DisplayPowerSetter::SetDisplaySoftwareDimming(bool dimmed) {
  LOG(INFO) << "Asking DisplayService to " << (dimmed ? "dim" : "undim")
            << " the display in software";
  dbus::MethodCall method_call(
      chromeos::kDisplayServiceInterface,
      chromeos::kDisplayServiceSetSoftwareDimmingMethod);
  dbus::MessageWriter writer(&method_call);
  writer.AppendBool(dimmed);
  dbus_wrapper_->CallMethodSync(
      display_service_proxy_, &method_call,
      base::TimeDelta::FromMilliseconds(kDisplayServiceDBusTimeoutMs));
}

void DisplayPowerSetter::SendStateToDisplayService(
    chromeos::DisplayPowerState state) {
  LOG(INFO) << "Asking DisplayService to turn "
            << DisplayPowerStateToString(state);
  dbus::MethodCall method_call(chromeos::kDisplayServiceInterface,
                               chromeos::kDisplayServiceSetPowerMethod);
  dbus::MessageWriter writer(&method_call);
  writer.AppendInt32(state);
  dbus_wrapper_->CallMethodSync(
      display_service_proxy_, &method_call,
      base::TimeDelta::FromMilliseconds(kDisplayServiceDBusTimeoutMs));
}

}  // namespace system
}  // namespace power_manager
