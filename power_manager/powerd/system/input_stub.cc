// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/powerd/system/input_stub.h"

#include "power_manager/powerd/system/input_observer.h"

namespace power_manager {
namespace system {

InputStub::InputStub()
    : lid_state_(LID_OPEN),
      usb_input_device_connected_(true),
      active_vt_(1),
      wake_inputs_enabled_(true) {
}

InputStub::~InputStub() {}

void InputStub::NotifyObserversAboutLidState() {
  FOR_EACH_OBSERVER(InputObserver, observers_, OnLidEvent(lid_state_));
}

void InputStub::NotifyObserversAboutPowerButtonEvent(ButtonState state) {
  FOR_EACH_OBSERVER(InputObserver, observers_, OnPowerButtonEvent(state));
}

void InputStub::AddObserver(InputObserver* observer) {
  DCHECK(observer);
  observers_.AddObserver(observer);
}

void InputStub::RemoveObserver(InputObserver* observer) {
  DCHECK(observer);
  observers_.RemoveObserver(observer);
}

LidState InputStub::QueryLidState() {
  return lid_state_;
}

bool InputStub::IsUSBInputDeviceConnected() const {
  return usb_input_device_connected_;
}

int InputStub::GetActiveVT() {
  return active_vt_;
}

void InputStub::SetInputDevicesCanWake(bool enable) {
  wake_inputs_enabled_ = enable;
}

}  // namespace system
}  // namespace power_manager
