// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/powerd/system/display_power_setter.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "dbus/message.h"
#include "dbus/object_proxy.h"
#include "power_manager/common/util.h"

namespace power_manager {
namespace system {

namespace {

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

DisplayPowerSetter::DisplayPowerSetter() : chrome_proxy_(NULL) {}

DisplayPowerSetter::~DisplayPowerSetter() {}

void DisplayPowerSetter::Init(dbus::ObjectProxy* chrome_proxy) {
  DCHECK(chrome_proxy);
  chrome_proxy_ = chrome_proxy;
}

void DisplayPowerSetter::SetDisplayPower(chromeos::DisplayPowerState state,
                                         base::TimeDelta delay) {
  if (delay.InMilliseconds() == 0) {
    timer_.Stop();
    SendStateToChrome(state);
  } else {
    timer_.Start(FROM_HERE, delay,
        base::Bind(&DisplayPowerSetter::SendStateToChrome,
                   base::Unretained(this), state));
  }
}

void DisplayPowerSetter::SetDisplaySoftwareDimming(bool dimmed) {
  LOG(INFO) << "Asking Chrome to " << (dimmed ? "dim" : "undim")
            << " the display in software";
  dbus::MethodCall method_call(chromeos::kLibCrosServiceInterface,
                               chromeos::kSetDisplaySoftwareDimming);
  dbus::MessageWriter writer(&method_call);
  writer.AppendBool(dimmed);
  scoped_ptr<dbus::Response> response(chrome_proxy_->CallMethodAndBlock(
      &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT));
}

void DisplayPowerSetter::SendStateToChrome(chromeos::DisplayPowerState state) {
  LOG(INFO) << "Asking Chrome to turn " << DisplayPowerStateToString(state);
  dbus::MethodCall method_call(chromeos::kLibCrosServiceInterface,
                               chromeos::kSetDisplayPower);
  dbus::MessageWriter writer(&method_call);
  writer.AppendInt32(state);
  scoped_ptr<dbus::Response> response(chrome_proxy_->CallMethodAndBlock(
      &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT));
}

}  // system
}  // power_manager
