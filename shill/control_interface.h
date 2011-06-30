// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_CONTROL_INTERFACE_
#define SHILL_CONTROL_INTERFACE_

namespace shill {

class Device;
class DeviceAdaptorInterface;
class Manager;
class ManagerAdaptorInterface;
class Profile;
class ProfileAdaptorInterface;
class Service;
class ServiceAdaptorInterface;

// This is the Interface for an object factory that creates adaptor objects
class ControlInterface {
 public:
  virtual ~ControlInterface() {}
  virtual ManagerAdaptorInterface *CreateManagerAdaptor(Manager *manager) = 0;
  virtual ServiceAdaptorInterface *CreateServiceAdaptor(Service *service) = 0;
  virtual DeviceAdaptorInterface *CreateDeviceAdaptor(Device *device) = 0;
  virtual ProfileAdaptorInterface *CreateProfileAdaptor(Profile *profile) = 0;
};

}  // namespace shill
#endif  // SHILL_CONTROL_INTERFACE_
