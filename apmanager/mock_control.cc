// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apmanager/mock_control.h"

namespace apmanager {

MockControl::MockControl() {}

MockControl::~MockControl() {}

std::unique_ptr<ConfigAdaptorInterface> MockControl::CreateConfigAdaptor(
      Config* /* config */, int /* service_identifier */) {
  return std::unique_ptr<ConfigAdaptorInterface>(CreateConfigAdaptorRaw());
}

std::unique_ptr<DeviceAdaptorInterface> MockControl::CreateDeviceAdaptor(
      Device* /* device */) {
  return std::unique_ptr<DeviceAdaptorInterface>(CreateDeviceAdaptorRaw());
}

std::unique_ptr<ManagerAdaptorInterface> MockControl::CreateManagerAdaptor(
      Manager* /* manager */) {
  return std::unique_ptr<ManagerAdaptorInterface>(CreateManagerAdaptorRaw());
}

std::unique_ptr<ServiceAdaptorInterface> MockControl::CreateServiceAdaptor(
      Service* /* service */) {
  return std::unique_ptr<ServiceAdaptorInterface>(CreateServiceAdaptorRaw());
}

std::unique_ptr<FirewallProxyInterface> MockControl::CreateFirewallProxy(
      const base::Closure& /* service_appeared_callback */,
      const base::Closure& /* service_vanished_callback */) {
  return std::unique_ptr<FirewallProxyInterface>(CreateFirewallProxyRaw());
}

std::unique_ptr<ShillProxyInterface> MockControl::CreateShillProxy(
      const base::Closure& /* service_appeared_callback */,
      const base::Closure& /* service_vanished_callback */) {
  return std::unique_ptr<ShillProxyInterface>(CreateShillProxyRaw());
}

}  // namespace apmanager
