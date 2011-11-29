// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MOCK_DHCP_PROVIDER_H_
#define SHILL_MOCK_DHCP_PROVIDER_H_

#include <string>

#include <base/basictypes.h>
#include <gmock/gmock.h>

#include "shill/dhcp_config.h"
#include "shill/dhcp_provider.h"
#include "shill/refptr_types.h"

namespace shill {

class MockDHCPProvider : public DHCPProvider {
 public:
  MockDHCPProvider();
  virtual ~MockDHCPProvider();

  MOCK_METHOD1(CreateConfig, DHCPConfigRefPtr(const std::string &device_name));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockDHCPProvider);
};

}  // namespace shill

#endif  // SHILL_MOCK_DHCP_PROVIDER_H_
