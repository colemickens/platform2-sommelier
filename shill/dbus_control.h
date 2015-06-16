// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_DBUS_CONTROL_H_
#define SHILL_DBUS_CONTROL_H_

#include <memory>

#include "shill/control_interface.h"

namespace DBus {
class Connection;
}

namespace shill {
// This is the Interface for the control channel for Shill.
class DBusControl : public ControlInterface {
 public:
  DBusControl();
  ~DBusControl() override;

  DeviceAdaptorInterface* CreateDeviceAdaptor(Device* device) override;
  IPConfigAdaptorInterface* CreateIPConfigAdaptor(IPConfig* ipconfig) override;
  ManagerAdaptorInterface* CreateManagerAdaptor(Manager* manager) override;
  ProfileAdaptorInterface* CreateProfileAdaptor(Profile* profile) override;
  RPCTaskAdaptorInterface* CreateRPCTaskAdaptor(RPCTask* task) override;
  ServiceAdaptorInterface* CreateServiceAdaptor(Service* service) override;
#ifndef DISABLE_VPN
  ThirdPartyVpnAdaptorInterface* CreateThirdPartyVpnAdaptor(
      ThirdPartyVpnDriver* driver) override;
#endif

  void Init();

 private:
  template <typename Object, typename AdaptorInterface, typename Adaptor>
  AdaptorInterface* CreateAdaptor(Object* object);
  DBus::Connection* GetConnection() const;
};

}  // namespace shill

#endif  // SHILL_DBUS_CONTROL_H_
