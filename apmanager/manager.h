// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APMANAGER_MANAGER_H_
#define APMANAGER_MANAGER_H_

#include <base/macros.h>
#include <base/memory/scoped_ptr.h>

#include "apmanager/dbus_adaptors/org.chromium.apmanager.Manager.h"

namespace chromeos {

namespace dbus_utils {
class ExportedObjectManager;
}  // namespace dbus_utils

}  // namespace chromeos

namespace apmanager {

class Manager : public org::chromium::apmanager::ManagerAdaptor,
                public org::chromium::apmanager::ManagerInterface {
 public:
  Manager();
  virtual ~Manager();

  // Register DBus object.
  void RegisterAsync(
      chromeos::dbus_utils::ExportedObjectManager* object_manager,
      chromeos::dbus_utils::AsyncEventSequencer* sequencer);

  // Implementation of ManagerInterface.
  virtual bool CreateService(chromeos::ErrorPtr* error,
                             dbus::ObjectPath* out_service);
  virtual bool RemoveService(chromeos::ErrorPtr* error,
                             const dbus::ObjectPath& in_service);

 private:
  friend class ManagerTest;

  std::unique_ptr<chromeos::dbus_utils::DBusObject> dbus_object_;

  DISALLOW_COPY_AND_ASSIGN(Manager);
};

}  // namespace apmanager

#endif  // APMANAGER_MANAGER_H_
