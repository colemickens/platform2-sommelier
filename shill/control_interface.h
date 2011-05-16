// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_CONTROL_INTERFACE_
#define SHILL_CONTROL_INTERFACE_

#include <string>

namespace shill {

class Device;
class Manager;
class Service;

using std::string;

// This is the Interface for "partner" objects which are in charge of
// handling incoming RPCs to the various core classes.
class AdaptorInterface {
 public:
  virtual void SetProperty(const string &key, const string &value) = 0;
  virtual const string *GetProperty(const string &key) = 0;
  virtual void ClearProperty(const string &key) = 0;
  virtual ~AdaptorInterface() {}
};

// These are the functions that a Manager adaptor must support
class ManagerAdaptorInterface {
 public:
  virtual void UpdateRunning() = 0;
  virtual ~ManagerAdaptorInterface() {}
};

// These are the functions that a Service adaptor must support
class ServiceAdaptorInterface {
 public:
  virtual void UpdateConnected() = 0;
  virtual ~ServiceAdaptorInterface() {}
};

// These are the functions that a Device adaptor must support
class DeviceAdaptorInterface {
 public:
  virtual void UpdateEnabled() = 0;
  virtual ~DeviceAdaptorInterface() {}
};

// This is the Interface for an object factory that creates adaptor objects
class ControlInterface {
 public:
  virtual ManagerAdaptorInterface *CreateManagerAdaptor(Manager *manager) = 0;
  virtual ServiceAdaptorInterface *CreateServiceAdaptor(Service *service) = 0;
  virtual DeviceAdaptorInterface *CreateDeviceAdaptor(Device *device) = 0;
  virtual ~ControlInterface() {}
};

}  // namespace shill
#endif  // SHILL_CONTROL_INTERFACE_
