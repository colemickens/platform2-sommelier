//
// Copyright (C) 2015 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#include "apmanager/dbus/manager_dbus_adaptor.h"

#if !defined(__ANDROID__)
#include <chromeos/dbus/service_constants.h>
#else
#include <dbus/apmanager/dbus-constants.h>
#endif  // __ANDROID__

#include "apmanager/manager.h"

using brillo::dbus_utils::ExportedObjectManager;
using org::chromium::apmanager::ManagerAdaptor;
using std::string;

namespace apmanager {

ManagerDBusAdaptor::ManagerDBusAdaptor(
    const scoped_refptr<dbus::Bus>& bus,
    ExportedObjectManager* object_manager,
    Manager* manager)
    : adaptor_(this),
      dbus_object_(object_manager, bus, ManagerAdaptor::GetObjectPath()),
      manager_(manager) {}

ManagerDBusAdaptor::~ManagerDBusAdaptor() {}

void ManagerDBusAdaptor::RegisterAsync(
    const base::Callback<void(bool)>& completion_callback) {
  adaptor_.RegisterWithDBusObject(&dbus_object_);
  dbus_object_.RegisterAsync(completion_callback);
}

bool ManagerDBusAdaptor::CreateService(brillo::ErrorPtr* error,
                                       dbus::Message* message,
                                       dbus::ObjectPath* out_service) {
  // TODO(zqiu): to be implemented.
  return true;
}

bool ManagerDBusAdaptor::RemoveService(brillo::ErrorPtr* error,
                                       dbus::Message* message,
                                       const dbus::ObjectPath& in_service) {
  // TODO(zqiu): to be implemented.
  return true;
}

}  // namespace apmanager
