// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_DBUS_CONTROL_
#define SHILL_DBUS_CONTROL_

#include <base/memory/scoped_ptr.h>
#include <dbus-c++/glib-integration.h>
#include <dbus-c++/util.h>

#include "shill/control_interface.h"

namespace shill {
// This is the Interface for the control channel for Shill.
class DBusControl : public ControlInterface {
 public:
  DBusControl();
  virtual ~DBusControl();

  virtual DeviceAdaptorInterface *CreateDeviceAdaptor(Device *device);
  virtual IPConfigAdaptorInterface *CreateIPConfigAdaptor(IPConfig *ipconfig);
  virtual ManagerAdaptorInterface *CreateManagerAdaptor(Manager *manager);
  virtual ProfileAdaptorInterface *CreateProfileAdaptor(Profile *profile);
  virtual RPCTaskAdaptorInterface *CreateRPCTaskAdaptor(RPCTask *task);
  virtual ServiceAdaptorInterface *CreateServiceAdaptor(Service *service);

  void Init();
  DBus::Connection *connection() { return connection_.get(); }

 private:
  scoped_ptr<DBus::Glib::BusDispatcher> dispatcher_;
  scoped_ptr<DBus::Connection> connection_;
};

}  // namespace shill

#endif  // SHILL_DBUS_CONTROL_
