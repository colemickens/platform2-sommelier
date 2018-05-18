// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bluetooth/dispatcher/dispatcher_daemon.h"

#include <base/logging.h>

namespace bluetooth {

bool DispatcherDaemon::Init(scoped_refptr<dbus::Bus> bus) {
  LOG(INFO) << "Bluetooth daemon started";

  suspend_manager_ = std::make_unique<SuspendManager>(bus);
  dispatcher_ = std::make_unique<Dispatcher>(bus);

  suspend_manager_->Init();

  return dispatcher_->Init();
}

}  // namespace bluetooth
