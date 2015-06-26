// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_DBUS_CHROMEOS_DBUS_CONTROL_H_
#define SHILL_DBUS_CHROMEOS_DBUS_CONTROL_H_

#include <memory>

#include <chromeos/dbus/exported_object_manager.h>

#include "shill/control_interface.h"

namespace shill {

// This is the Interface for the  DBus control channel for Shill.
class ChromeosDBusControl : public ControlInterface {
 public:
  ChromeosDBusControl(
      const base::WeakPtr<chromeos::dbus_utils::ExportedObjectManager>&
          object_manager,
      const scoped_refptr<dbus::Bus>& bus);
  ~ChromeosDBusControl() override;

  DeviceAdaptorInterface* CreateDeviceAdaptor(Device* device) override;

 private:
  template <typename Object, typename AdaptorInterface, typename Adaptor>
  AdaptorInterface* CreateAdaptor(Object* object);

  base::WeakPtr<chromeos::dbus_utils::ExportedObjectManager> object_manager_;
  scoped_refptr<dbus::Bus> bus_;
};

}  // namespace shill

#endif  // SHILL_DBUS_CHROMEOS_DBUS_CONTROL_H_
