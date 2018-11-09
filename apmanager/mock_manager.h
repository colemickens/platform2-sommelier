// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APMANAGER_MOCK_MANAGER_H_
#define APMANAGER_MOCK_MANAGER_H_

#include <string>

#include <base/macros.h>
#include <gmock/gmock.h>

#include "apmanager/manager.h"

namespace apmanager {

class MockManager : public Manager {
 public:
  explicit MockManager(ControlInterface* control_interface);
  ~MockManager() override;

  MOCK_METHOD0(Start, void());
  MOCK_METHOD0(Stop, void());
  MOCK_METHOD1(RegisterDevice, void(const scoped_refptr<Device>& device));
  MOCK_METHOD0(GetAvailableDevice, scoped_refptr<Device>());
  MOCK_METHOD1(GetDeviceFromInterfaceName,
               scoped_refptr<Device>(const std::string& interface_name));
  MOCK_METHOD1(ClaimInterface, void(const std::string& interface_name));
  MOCK_METHOD1(ReleaseInterface, void(const std::string& interface_name));
  MOCK_METHOD1(RequestDHCPPortAccess, void(const std::string& interface));
  MOCK_METHOD1(ReleaseDHCPPortAccess, void(const std::string& interface));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockManager);
};

}  // namespace apmanager

#endif  // APMANAGER_MOCK_MANAGER_H_
