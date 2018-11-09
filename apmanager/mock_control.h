// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APMANAGER_MOCK_CONTROL_H_
#define APMANAGER_MOCK_CONTROL_H_

#include <memory>

#include <base/macros.h>
#include <gmock/gmock.h>

#include "apmanager/control_interface.h"

namespace apmanager {

class MockControl : public ControlInterface {
 public:
  MockControl();
  ~MockControl() override;

  MOCK_METHOD0(Init, void());
  MOCK_METHOD0(Shutdown, void());

  // Provide mock methods for creating raw pointer for adaptor/proxy.
  // This allows us to set expectations for adaptor/proxy creation
  // functions, since mock methods only support copyable return values,
  // and unique_ptr is not copyable.
  MOCK_METHOD0(CreateConfigAdaptorRaw, ConfigAdaptorInterface*());
  MOCK_METHOD0(CreateDeviceAdaptorRaw, DeviceAdaptorInterface*());
  MOCK_METHOD0(CreateFirewallProxyRaw, FirewallProxyInterface*());
  MOCK_METHOD0(CreateManagerAdaptorRaw, ManagerAdaptorInterface*());
  MOCK_METHOD0(CreateServiceAdaptorRaw, ServiceAdaptorInterface*());
  MOCK_METHOD0(CreateShillProxyRaw, ShillProxyInterface*());

  // These functions use the mock methods above for creating
  // raw object.
  std::unique_ptr<ConfigAdaptorInterface> CreateConfigAdaptor(
      Config* config, int service_identifier) override;
  std::unique_ptr<DeviceAdaptorInterface> CreateDeviceAdaptor(
      Device* device) override;
  std::unique_ptr<ManagerAdaptorInterface> CreateManagerAdaptor(
      Manager* manager) override;
  std::unique_ptr<ServiceAdaptorInterface> CreateServiceAdaptor(
      Service* service) override;
  std::unique_ptr<FirewallProxyInterface> CreateFirewallProxy(
      const base::Closure& service_appeared_callback,
      const base::Closure& service_vanished_callback) override;
  std::unique_ptr<ShillProxyInterface> CreateShillProxy(
      const base::Closure& service_appeared_callback,
      const base::Closure& service_vanished_callback) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(MockControl);
};

}  // namespace apmanager

#endif  // APMANAGER_MOCK_CONTROL_H_
