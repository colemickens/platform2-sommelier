// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bluetooth/newblued/newblue_daemon.h"

#include <base/logging.h>
#include <chromeos/dbus/service_constants.h>

namespace bluetooth {

void NewblueDaemon::Init(scoped_refptr<dbus::Bus> bus) {
  bus_ = bus;

  if (!bus_->RequestOwnershipAndBlock(
          newblue_object_manager::kNewblueObjectManagerServiceName,
          dbus::Bus::REQUIRE_PRIMARY)) {
    LOG(ERROR) << "Failed to acquire D-Bus name ownership: "
               << newblue_object_manager::kNewblueObjectManagerServiceName;
  }

  LOG(INFO) << "newblued initialized";
}

}  // namespace bluetooth
