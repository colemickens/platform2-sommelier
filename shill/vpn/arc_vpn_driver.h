// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_VPN_ARC_VPN_DRIVER_H_
#define SHILL_VPN_ARC_VPN_DRIVER_H_

#include <string>

#include <base/callback.h>
#include <gtest/gtest_prod.h>

#include "shill/device_info.h"
#include "shill/error.h"
#include "shill/ipconfig.h"
#include "shill/refptr_types.h"
#include "shill/service.h"
#include "shill/virtual_device.h"
#include "shill/vpn/vpn_driver.h"

namespace shill {

class ArcVpnDriver : public VPNDriver {
 public:
  ArcVpnDriver(Manager* manager, DeviceInfo* device_info);
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

  DeviceInfo* device_info_;

  VPNServiceRefPtr service_;
  VirtualDeviceRefPtr device_;

  DISALLOW_COPY_AND_ASSIGN(ArcVpnDriver);
};

}  // namespace shill

#endif  // SHILL_VPN_ARC_VPN_DRIVER_H_
