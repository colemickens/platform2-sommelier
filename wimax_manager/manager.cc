// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "wimax_manager/manager.h"

#include <dbus-c++/dbus.h>

#include <base/logging.h>
#include <base/stl_util.h>

#include "wimax_manager/device.h"
#include "wimax_manager/device_dbus_adaptor.h"
#include "wimax_manager/gdm_driver.h"
#include "wimax_manager/manager_dbus_adaptor.h"

namespace wimax_manager {

Manager::Manager(DBus::Connection *dbus_connection)
    : dbus_connection_(dbus_connection),
      adaptor_(new(std::nothrow) ManagerDBusAdaptor(dbus_connection, this)) {
  CHECK(dbus_connection_);
  CHECK(adaptor_.get());
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

  if (!driver_->GetDevices(&devices_)) {
    LOG(ERROR) << "Failed to get list of devices";
    return false;
  }

  for (size_t i = 0; i < devices_.size(); ++i) {
    Device *device = devices_[i];
    DeviceDBusAdaptor *adaptor =
        new(std::nothrow) DeviceDBusAdaptor(dbus_connection_, device);
    CHECK(adaptor);
    device->set_adaptor(adaptor);
  }
  adaptor_->UpdateDevices();

  return true;
}

bool Manager::Finalize() {
  STLDeleteElements(&devices_);

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
