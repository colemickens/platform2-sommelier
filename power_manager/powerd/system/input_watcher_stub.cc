// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/powerd/system/input_watcher_stub.h"

#include "power_manager/powerd/system/input_observer.h"

namespace power_manager {
namespace system {

InputWatcherStub::InputWatcherStub()
    : lid_state_(LID_OPEN),
      tablet_mode_(TABLET_MODE_OFF),
      usb_input_device_connected_(true),
      active_vt_(1) {
}

InputWatcherStub::~InputWatcherStub() {}

void InputWatcherStub::NotifyObserversAboutLidState() {
  FOR_EACH_OBSERVER(InputObserver, observers_, OnLidEvent(lid_state_));
}

void InputWatcherStub::NotifyObserversAboutTabletMode() {
  FOR_EACH_OBSERVER(InputObserver, observers_, OnTabletModeEvent(tablet_mode_));
}

void InputWatcherStub::NotifyObserversAboutPowerButtonEvent(ButtonState state) {
  FOR_EACH_OBSERVER(InputObserver, observers_, OnPowerButtonEvent(state));
}

void InputWatcherStub::NotifyObserversAboutHoverState(bool hovering) {
  FOR_EACH_OBSERVER(InputObserver, observers_, OnHoverStateChanged(hovering));
}

void InputWatcherStub::AddObserver(InputObserver* observer) {
  DCHECK(observer);
  observers_.AddObserver(observer);
}

void InputWatcherStub::RemoveObserver(InputObserver* observer) {
  DCHECK(observer);
  observers_.RemoveObserver(observer);
}

LidState InputWatcherStub::QueryLidState() {
  return lid_state_;
}

TabletMode InputWatcherStub::GetTabletMode() {
  return tablet_mode_;
}

bool InputWatcherStub::IsUSBInputDeviceConnected() const {
  return usb_input_device_connected_;
}

int InputWatcherStub::GetActiveVT() {
  return active_vt_;
}

}  // namespace system
}  // namespace power_manager
