// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APMANAGER_CONTROL_INTERFACE_H_
#define APMANAGER_CONTROL_INTERFACE_H_

#include <base/callback.h>
#include <base/macros.h>

#include "apmanager/config_adaptor_interface.h"
#include "apmanager/device_adaptor_interface.h"
#include "apmanager/firewall_proxy_interface.h"
#include "apmanager/manager_adaptor_interface.h"
#include "apmanager/service_adaptor_interface.h"
#include "apmanager/shill_proxy_interface.h"

namespace apmanager {

class Config;
class Device;
class Manager;
class Service;

// This is the Interface for an object factory that creates adaptor/proxy
// objects
class ControlInterface {
 public:
  virtual ~ControlInterface() {}

  virtual void Init() = 0;
  virtual void Shutdown() = 0;

  // Adaptor creation APIs.
  virtual std::unique_ptr<ConfigAdaptorInterface> CreateConfigAdaptor(
      Config* config, int service_identifier) = 0;
  virtual std::unique_ptr<DeviceAdaptorInterface> CreateDeviceAdaptor(
      Device* device) = 0;
  virtual std::unique_ptr<ManagerAdaptorInterface> CreateManagerAdaptor(
      Manager* manager) = 0;
  virtual std::unique_ptr<ServiceAdaptorInterface> CreateServiceAdaptor(
      Service* service) = 0;

  // Proxy creation APIs.
  virtual std::unique_ptr<FirewallProxyInterface> CreateFirewallProxy(
      const base::Closure& service_appeared_callback,
      const base::Closure& service_vanished_callback) = 0;
  virtual std::unique_ptr<ShillProxyInterface> CreateShillProxy(
      const base::Closure& service_appeared_callback,
      const base::Closure& service_vanished_callback) = 0;
};

}  // namespace apmanager

#endif  // APMANAGER_CONTROL_INTERFACE_H_
