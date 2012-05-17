// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "wimax_manager/manager.h"

#include <base/logging.h>
#include <base/stl_util.h>

#include "wimax_manager/device.h"
#include "wimax_manager/device_dbus_adaptor.h"
#include "wimax_manager/gdm_driver.h"
#include "wimax_manager/manager_dbus_adaptor.h"

namespace wimax_manager {

Manager::Manager() {
}

Manager::~Manager() {
  Finalize();
}

bool Manager::Initialize() {
  if (driver_.get())
    return true;

  driver_.reset(new(std::nothrow) GdmDriver());
  if (!driver_.get()) {
    LOG(ERROR) << "Failed to create driver";
    return false;
  }

  if (!driver_->Initialize()) {
    LOG(ERROR) << "Failed to initialize driver";
    return false;
  }

  if (!driver_->GetDevices(&devices_.get())) {
    LOG(ERROR) << "Failed to get list of devices";
    return false;
  }

  for (size_t i = 0; i < devices_.size(); ++i)
    devices_[i]->CreateDBusAdaptor();

  dbus_adaptor()->UpdateDevices();
  return true;
}

bool Manager::Finalize() {
  devices_.reset();

  if (!driver_.get())
    return true;

  if (!driver_->Finalize()) {
    LOG(ERROR) << "Failed to de-initialize driver";
    return false;
  }

  driver_.reset();
  return true;
}

}  // namespace wimax_manager
