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

#ifndef APMANAGER_DBUS_MANAGER_DBUS_ADAPTOR_H_
#define APMANAGER_DBUS_MANAGER_DBUS_ADAPTOR_H_

#include <base/macros.h>
#include <dbus_bindings/org.chromium.apmanager.Manager.h>

#include "apmanager/manager_adaptor_interface.h"

namespace apmanager {

class Manager;

class ManagerDBusAdaptor : public org::chromium::apmanager::ManagerInterface,
                           public ManagerAdaptorInterface {
 public:
  ManagerDBusAdaptor(const scoped_refptr<dbus::Bus>& bus,
                     brillo::dbus_utils::ExportedObjectManager* object_manager,
                     Manager* manager);
  ~ManagerDBusAdaptor() override;

  // Implementation of org::chromium::apmanager::ManagerInterface.
  bool CreateService(brillo::ErrorPtr* error,
                     dbus::Message* message,
                     dbus::ObjectPath* out_service) override;
  bool RemoveService(brillo::ErrorPtr* error,
                     dbus::Message* message,
                     const dbus::ObjectPath& in_service) override;

  // Implementation of ManagerAdaptorInterface.
  void RegisterAsync(
      const base::Callback<void(bool)>& completion_callback) override;

 private:
  org::chromium::apmanager::ManagerAdaptor adaptor_;
  brillo::dbus_utils::DBusObject dbus_object_;
  Manager* manager_;

  DISALLOW_COPY_AND_ASSIGN(ManagerDBusAdaptor);
};

}  // namespace apmanager

#endif  // APMANAGER_DBUS_MANAGER_DBUS_ADAPTOR_H_
