//
// Copyright 2017 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#ifndef SHILL_VPN_ARC_VPN_DRIVER_H_
#define SHILL_VPN_ARC_VPN_DRIVER_H_

#include <string>

#include <base/callback.h>
#include <gtest/gtest_prod.h>

#include "shill/control_interface.h"
#include "shill/device_info.h"
#include "shill/error.h"
#include "shill/ipconfig.h"
#include "shill/manager.h"
#include "shill/metrics.h"
#include "shill/refptr_types.h"
#include "shill/service.h"
#include "shill/virtual_device.h"
#include "shill/vpn/vpn_driver.h"

namespace shill {

class ArcVpnDriver : public VPNDriver {
 public:
  ArcVpnDriver(ControlInterface* control,
               EventDispatcher* dispatcher,
               Metrics* metrics,
               Manager* manager,
               DeviceInfo* device_info);
  ~ArcVpnDriver() override;

  // Implementation of VPNDriver
  bool ClaimInterface(const std::string& link_name,
                      int interface_index) override;
  void Connect(const VPNServiceRefPtr& service, Error* error) override;
  std::string GetProviderType() const override;
  void Disconnect() override;
  void OnConnectionDisconnected() override;

 private:
  friend class ArcVpnDriverTest;

  // Tear down the connection.
  void Cleanup();

  static const Property kProperties[];

  ControlInterface* control_;
  EventDispatcher* dispatcher_;
  Metrics* metrics_;
  DeviceInfo* device_info_;

  VPNServiceRefPtr service_;
  VirtualDeviceRefPtr device_;

  DISALLOW_COPY_AND_ASSIGN(ArcVpnDriver);
};

}  // namespace shill

#endif  // SHILL_VPN_ARC_VPN_DRIVER_H_
