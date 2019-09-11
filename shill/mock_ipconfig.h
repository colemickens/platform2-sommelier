// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MOCK_IPCONFIG_H_
#define SHILL_MOCK_IPCONFIG_H_

#include <string>
#include <vector>

#include <base/macros.h>
#include <gmock/gmock.h>

#include "shill/ipconfig.h"

namespace shill {

class MockIPConfig : public IPConfig {
 public:
  MockIPConfig(ControlInterface* control_interface,
               const std::string& device_name);
  ~MockIPConfig() override;

  MOCK_CONST_METHOD0(properties, const Properties&());
  MOCK_METHOD0(RequestIP, bool());
  MOCK_METHOD0(RenewIP, bool());
  MOCK_METHOD1(ReleaseIP, bool(ReleaseReason reason));
  MOCK_METHOD1(Refresh, void(Error* error));
  MOCK_METHOD0(ResetProperties, void());
  MOCK_METHOD0(EmitChanges, void());
  MOCK_METHOD1(UpdateDNSServers,
               void(const std::vector<std::string>& dns_servers));
  MOCK_METHOD1(UpdateLeaseExpirationTime, void(uint32_t new_lease_duration));
  MOCK_METHOD0(ResetLeaseExpirationTime, void());

 private:
  const Properties& real_properties() const { return IPConfig::properties(); }

  DISALLOW_COPY_AND_ASSIGN(MockIPConfig);
};

}  // namespace shill

#endif  // SHILL_MOCK_IPCONFIG_H_
