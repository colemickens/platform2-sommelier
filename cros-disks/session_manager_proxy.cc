// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cros-disks/session_manager_proxy.h"

#include <base/bind.h>

namespace cros_disks {

namespace {

void OnSignalConnected(const std::string& interface,
                       const std::string& signal,
                       bool success) {
  if (!success) {
    LOG(ERROR) << "Could not connect to signal " << signal << " on interface "
               << interface;
  }
}

}  // namespace

SessionManagerProxy::SessionManagerProxy(scoped_refptr<dbus::Bus> bus)
    : proxy_(bus), weak_ptr_factory_(this) {
  proxy_.RegisterScreenIsLockedSignalHandler(
      base::Bind(&SessionManagerProxy::OnScreenIsLocked,
                 weak_ptr_factory_.GetWeakPtr()),
      base::Bind(&OnSignalConnected));
  proxy_.RegisterScreenIsUnlockedSignalHandler(
      base::Bind(&SessionManagerProxy::OnScreenIsUnlocked,
                 weak_ptr_factory_.GetWeakPtr()),
      base::Bind(&OnSignalConnected));
  proxy_.RegisterSessionStateChangedSignalHandler(
      base::Bind(&SessionManagerProxy::OnSessionStateChanged,
                 weak_ptr_factory_.GetWeakPtr()),
      base::Bind(&OnSignalConnected));
}

void SessionManagerProxy::AddObserver(
    SessionManagerObserverInterface* observer) {
  CHECK(observer) << "Invalid observer object";
  observer_list_.AddObserver(observer);
}

void SessionManagerProxy::OnScreenIsLocked() {
  FOR_EACH_OBSERVER(SessionManagerObserverInterface, observer_list_,
                    OnScreenIsLocked());
}

void SessionManagerProxy::OnScreenIsUnlocked() {
  FOR_EACH_OBSERVER(SessionManagerObserverInterface, observer_list_,
                    OnScreenIsUnlocked());
}

void SessionManagerProxy::OnSessionStateChanged(const std::string& state) {
  if (state == "started") {
    FOR_EACH_OBSERVER(SessionManagerObserverInterface, observer_list_,
                      OnSessionStarted());
  } else if (state == "stopped") {
    FOR_EACH_OBSERVER(SessionManagerObserverInterface, observer_list_,
                      OnSessionStopped());
  }
}

}  // namespace cros_disks
