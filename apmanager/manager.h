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

class Manager
    : public org::chromium::apmanager::ManagerAdaptor::MethodInterface {
 public:
  Manager();
  virtual ~Manager();

  // Start DBus connection.
  void InitDBus(chromeos::dbus_utils::ExportedObjectManager* object_manager);

  // Implementation of MethodInterface.
  virtual dbus::ObjectPath CreateService(chromeos::ErrorPtr* error);
  virtual void RemoveService(chromeos::ErrorPtr* error,
                             const dbus::ObjectPath& service);

 private:
  friend class ManagerTest;

  std::unique_ptr<org::chromium::apmanager::ManagerAdaptor> dbus_adaptor_;

  DISALLOW_COPY_AND_ASSIGN(Manager);
};

}  // namespace apmanager

#endif  // APMANAGER_MANAGER_H_
