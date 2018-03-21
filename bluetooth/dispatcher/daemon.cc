// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bluetooth/dispatcher/daemon.h"

#include <base/logging.h>

namespace bluetooth {

Daemon::Daemon(scoped_refptr<dbus::Bus> bus)
    : suspend_manager_(std::make_unique<SuspendManager>(bus)) {}

Daemon::~Daemon() = default;

void Daemon::Init() {
  LOG(INFO) << "Bluetooth daemon started";

  suspend_manager_->Init();
}

}  // namespace bluetooth
