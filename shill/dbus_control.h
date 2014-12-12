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

  virtual DeviceAdaptorInterface *CreateDeviceAdaptor(Device *device);
  virtual IPConfigAdaptorInterface *CreateIPConfigAdaptor(IPConfig *ipconfig);
  virtual ManagerAdaptorInterface *CreateManagerAdaptor(Manager *manager);
  virtual ProfileAdaptorInterface *CreateProfileAdaptor(Profile *profile);
  virtual RPCTaskAdaptorInterface *CreateRPCTaskAdaptor(RPCTask *task);
  virtual ServiceAdaptorInterface *CreateServiceAdaptor(Service *service);
#ifndef DISABLE_VPN
  virtual ThirdPartyVpnAdaptorInterface *CreateThirdPartyVpnAdaptor(
      ThirdPartyVpnDriver *driver);
#endif

  void Init();

 private:
  template <typename Object, typename AdaptorInterface, typename Adaptor>
  AdaptorInterface *CreateAdaptor(Object *object);
  DBus::Connection *GetConnection() const;
};

}  // namespace shill

#endif  // SHILL_DBUS_CONTROL_H_
