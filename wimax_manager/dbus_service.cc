// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "wimax_manager/dbus_service.h"

#include <chromeos/dbus/service_constants.h>

#include "wimax_manager/dbus_service_dbus_proxy.h"
#include "wimax_manager/manager.h"
#include "wimax_manager/power_manager.h"
#include "wimax_manager/power_manager_dbus_proxy.h"

using std::string;

namespace wimax_manager {

DBusService::DBusService(Manager *manager) : manager_(manager) {
  CHECK(manager_);
}

DBusService::~DBusService() {
  Finalize();
}

void DBusService::Initialize() {
  if (NameHasOwner(power_manager::kPowerManagerServiceName))
    SetPowerManager(new(std::nothrow) PowerManager(manager_));
}

void DBusService::Finalize() {
  power_manager_.reset();
}

bool DBusService::NameHasOwner(const std::string &name) {
  if (!dbus_proxy())
    return false;

  try {
    return dbus_proxy()->NameHasOwner(name);
  } catch (const DBus::Error &error) {
    LOG(ERROR) << "Failed to check if a DBus name has a owner. DBus exception: "
               << error.name() << ": " << error.what();
  }
  return false;
}

void DBusService::OnNameOwnerChanged(const std::string &name,
                                     const std::string &old_owner,
                                     const std::string &new_owner) {
  if (name == power_manager::kPowerManagerServiceName) {
    LOG(INFO) << "Owner of '" << name << "' changed from '"
              << old_owner << "' to '" << new_owner << "'.";
    if (new_owner.empty())
      SetPowerManager(NULL);
    else
      SetPowerManager(new(std::nothrow) PowerManager(manager_));
  }
}

void DBusService::SetPowerManager(PowerManager *power_manager) {
  // The old power manager instance no longer exists, invalidate its DBus
  // proxy to prevent calling UnregisterSuspendDelay on it at destruction.
  if (power_manager_.get()) {
    LOG(INFO) << "Destroy old power manager proxy.";
    power_manager_->InvalidateDBusProxy();
    power_manager_.reset();
  }

  if (!power_manager)
    return;

  LOG(INFO) << "Create a new power manager proxy.";
  power_manager_.reset(power_manager);
  CHECK(power_manager_.get());
  power_manager_->CreateDBusProxy();
  power_manager_->Initialize();
}

}  // namespace wimax_manager
