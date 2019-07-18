// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MOCK_VIRTUAL_DEVICE_H_
#define SHILL_MOCK_VIRTUAL_DEVICE_H_

#include <string>

#include <gmock/gmock.h>

#include "shill/virtual_device.h"

namespace shill {

class MockVirtualDevice : public VirtualDevice {
 public:
  MockVirtualDevice(Manager* manager,
                    const std::string& link_name,
                    int interface_index,
                    Technology technology);
  ~MockVirtualDevice() override;

  MOCK_METHOD2(Stop,
               void(Error* error, const EnabledStateChangedCallback& callback));
  MOCK_METHOD1(UpdateIPConfig,
               void(const IPConfig::Properties& properties));
  MOCK_METHOD0(DropConnection, void());
  MOCK_METHOD0(ResetConnection, void());
  MOCK_METHOD1(SetServiceState, void(Service::ConnectState state));
  MOCK_METHOD1(SetEnabled, void(bool));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockVirtualDevice);
};

}  // namespace shill

#endif  // SHILL_MOCK_VIRTUAL_DEVICE_H_
