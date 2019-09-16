// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MOCK_PPP_DEVICE_H_
#define SHILL_MOCK_PPP_DEVICE_H_

#include <map>
#include <string>

#include <gmock/gmock.h>

#include "shill/ppp_device.h"

namespace shill {

class MockPPPDevice : public PPPDevice {
 public:
  MockPPPDevice(Manager* manager,
                const std::string& link_name,
                int interface_index);
  ~MockPPPDevice() override;

  MOCK_METHOD2(Stop,
               void(Error* error, const EnabledStateChangedCallback& callback));
  MOCK_METHOD1(UpdateIPConfig, void(const IPConfig::Properties& properties));
  MOCK_METHOD0(DropConnection, void());
  MOCK_METHOD1(SelectService, void(const ServiceRefPtr& service));
  MOCK_METHOD1(SetServiceState, void(Service::ConnectState));
  MOCK_METHOD1(SetServiceFailure, void(Service::ConnectFailure));
  MOCK_METHOD1(SetServiceFailureSilent, void(Service::ConnectFailure));
  MOCK_METHOD1(SetEnabled, void(bool));
  MOCK_METHOD2(UpdateIPConfigFromPPP,
               void(const std::map<std::string, std::string>& config,
                    bool blackhole_ipv6));
#ifndef DISABLE_DHCPV6
  MOCK_METHOD0(AcquireIPv6Config, bool());
#endif  // DISABLE_DHCPV6

 private:
  DISALLOW_COPY_AND_ASSIGN(MockPPPDevice);
};

}  // namespace shill

#endif  // SHILL_MOCK_PPP_DEVICE_H_
