// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_DHCP_MOCK_DHCP_PROVIDER_H_
#define SHILL_DHCP_MOCK_DHCP_PROVIDER_H_

#include <string>

#include <base/macros.h>
#include <gmock/gmock.h>

#include "shill/dhcp/dhcp_config.h"
#include "shill/dhcp/dhcp_properties.h"
#include "shill/dhcp/dhcp_provider.h"
#include "shill/refptr_types.h"

namespace shill {

class MockDHCPProvider : public DHCPProvider {
 public:
  MockDHCPProvider();
  ~MockDHCPProvider() override;

  MOCK_METHOD3(Init, void(ControlInterface*, EventDispatcher*, Metrics*));
  MOCK_METHOD4(CreateIPv4Config,
               DHCPConfigRefPtr(const std::string& device_name,
                                const std::string& storage_identifier,
                                bool arp_gateway,
                                const DhcpProperties& dhcp_props));
#ifndef DISABLE_DHCPV6
  MOCK_METHOD2(CreateIPv6Config,
               DHCPConfigRefPtr(const std::string& device_name,
                                const std::string& storage_identifier));
#endif
  MOCK_METHOD2(BindPID, void(int pid, const DHCPConfigRefPtr& config));
  MOCK_METHOD1(UnbindPID, void(int pid));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockDHCPProvider);
};

}  // namespace shill

#endif  // SHILL_DHCP_MOCK_DHCP_PROVIDER_H_
