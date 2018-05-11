// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bluetooth/dispatcher/dispatcher_daemon.h"

#include <base/logging.h>

namespace bluetooth {

DispatcherDaemon::DispatcherDaemon(PassthroughMode passthrough_mode)
    : passthrough_mode_(passthrough_mode) {}

bool DispatcherDaemon::Init(scoped_refptr<dbus::Bus> bus,
                            DBusDaemon* dbus_daemon) {
  LOG(INFO) << "Bluetooth daemon started";

  suspend_manager_ = std::make_unique<SuspendManager>(bus);
  dispatcher_ = std::make_unique<Dispatcher>(bus);

  suspend_manager_->Init();

  return dispatcher_->Init(passthrough_mode_);
}

}  // namespace bluetooth
